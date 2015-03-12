// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#include "query_term.h"

QueryTerm::QueryTerm(IndexFile* fd, termid_t termid, offset_t o):m_fd(fd), m_offset(o), m_cursor(o),
    m_block_idx(-1), m_pagelist_idx(0), m_termid(termid)
{
    m_pagelist.reserve(kCacheSize);
    init();
}

bool QueryTerm::next(pageid_t *pageid)
{
    // 顺序扫
    if (++m_pagelist_idx >= m_pagelist.size())
    {
        read_next_block();
    }
    if (m_pagelist.empty())
    {
        return false;
    }
    *pageid = m_pagelist[m_pagelist_idx];
    return true;
}

bool QueryTerm::seek_lower_bound(pageid_t* bound, const pageid_t pageid)  // >=
{
    // seek的时候只能先seek小的, 再seek大的, 反之不行
    // 根据跳表确定termid所在的block, 如果当前块没有加载,立即加载进来
    skipair_t needle;
    needle.first = pageid;
    skiplist_t::const_iterator it_skiplist = std::lower_bound(m_skiplist.begin(),
                                                              m_skiplist.end(), needle, skipair_cmp_function);
    if (it_skiplist == m_skiplist.end()) {
        return false;
    }
    int32_t index = it_skiplist - m_skiplist.begin();
    // 第index 个block是否已经加载
    if (index != get_block_index())
    {
        read_block(index);
    }
    // pagelist必须是ascending序
    pagelist_t::const_iterator it_pageid = std::lower_bound(m_pagelist.begin(), m_pagelist.end(), pageid);
    if (it_pageid == m_pagelist.end())
    {
        return false;
    }
    *bound = *it_pageid;
    m_pagelist_idx = it_pageid - m_pagelist.begin();
    return true;
}

bool QueryTerm::init()
{
    char buff[kHeaderSize + 1] = {0};    // 块缓存
    size_t nread = m_fd->read(buff, kHeaderSize, m_cursor);
    buff[nread] = '\0';
    // 先把df给弄出来
    size_t offset = decode_df(buff);
    bool ret(false);
    if (offset < 2) {
        LOG(ERROR) << m_fd->get_fname() << " decode_df failed!";
    } else {
        // decode skiplist
        offset += decode_skiplist(buff + offset);
        // curosr已经指向了doclist的第一个block
        ret = true;
    }
    m_cursor += offset;
    return ret;
}

size_t QueryTerm::read_next_block()
{
    return read_block(get_block_index() + 1);
}

size_t QueryTerm::read_block(int32_t block_index)
{
    m_pagelist_idx = 0;
    if (block_index < int32_t(m_skiplist.size()) - 1)
    {
        set_block_index(block_index);
        offset_t cursor = get_block_cursor(block_index);
        size_t size = get_block_size(block_index);
        scoped_array<char> data(new char[size + 1]);
        size_t nread = m_fd->read(data.get(), size, cursor);
        data[nread] = '\0';
        size_t record_len = decode_block_pagelist(data.get());
        size_t record_num = m_pagelist.size();
        LOG(INFO) << m_termid << "[" << block_index << "] have " << record_num << " records,len=" << record_len;
        return record_num;
    }
    return 0;
}

offset_t QueryTerm::get_block_cursor(int32_t block_index)
{
    assert(block_index >= 0 && block_index < int32_t(m_skiplist.size()) - 1);
    return m_cursor + m_skiplist[block_index].second;
}
size_t QueryTerm::get_block_size(int32_t block_index)
{
    // 根据跳表计算每个block的size
    assert(block_index < m_skiplist.size() - 1);
    return m_skiplist[block_index+1].second - m_skiplist[block_index].second;
}

size_t QueryTerm::decode_skiplist(char *data)
{
    char *orig(data);
    char *pfind = strstr(data, kDelim);
    if (!pfind)
        throw std::runtime_error(StringFormat("%s:%d:%s faield with %s",
                                              __FILE__, __LINE__, __FUNCTION__, data));
    size_t skiplist_length(0);
    StringToNumber(data, &skiplist_length, kValueBase);
    if (skiplist_length < 1)
        throw std::runtime_error(StringFormat("%s:%d:%s faield with %s",
                                              __FILE__, __LINE__, __FUNCTION__, data));
    size_t offset = pfind + strlen(kDelim) - data;
    data += offset;

    // prepare skiplist data
    if (skiplist_length <= strlen(data))
    {
        // 不需要重新读数据了, skiplist的length包含了最后的一个逗号(kDelim), 最后一个数据不需要特殊处理
        size_t i(0);
        data[skiplist_length] = '\0';
        skipair_t skipair;
        while ( (pfind = strstr(data, kDelim)) != NULL )
        {
            *pfind = '\0';
            if ( i++ % 2 == 0 )
            {
                StringToNumber(data, &skipair.first, kValueBase);
            }
            else
            {
                StringToNumber(data, &skipair.second, kValueBase);
                m_skiplist.push_back(skipair);
            }
            data = pfind + strlen(kDelim);
        }
        offset = data - orig;
    }
    else
    {
        scoped_ptr<char> buff(new char[skiplist_length + 1]);    // 块缓存
        size_t nread = m_fd->read(buff.get(), skiplist_length, m_cursor+offset);
        data = buff.get();
        data[nread] = '\0';
        skipair_t skipair;
        size_t i(0);
        while ( (pfind = strstr(data, kDelim)) != NULL )
        {
            *pfind = '\0';
            if ( i % 2 == 0 )
            {
                StringToNumber(data, &skipair.first, kValueBase);
            }
            else
            {
                StringToNumber(data, &skipair.second, kValueBase);
                m_skiplist.push_back(skipair);
            }
            data = pfind + strlen(kDelim);
        }
        offset += data - buff.get();
    }
    assert(m_skiplist.size() > 1);
    LOG(INFO) << "skiplist.size()=" << m_skiplist.size();
    return offset;
}

size_t QueryTerm::decode_block_pagelist(char *data)
{
    // 差分压缩了
    // doclist必须是ascending序
    char *orig(data);
    char *pfind = strstr(data, kDelim);
    if (!pfind)
        throw std::runtime_error(StringFormat("%s:%d:%s faield with %s",
                                              __FILE__, __LINE__, __FUNCTION__, data));
    pageid_t last_pageid(0);
    m_pagelist.clear();
    while ( (pfind = strstr(data, kDelim)) != NULL )
    {
        *pfind = '\0';
        pageid_t pageid = static_cast<pageid_t>(strtoull(data, NULL, kValueBase));
        // StringToNumber(data, &pageid, kValueBase);
        last_pageid += pageid;
        m_pagelist.push_back(last_pageid);
        data = pfind + strlen(kDelim);
    }
    return data - orig;
}

size_t QueryTerm::decode_df(char *data)
{
    // 返回解压了多少字节数据
    char *pfind = strstr(data, kDelim);
    if (!pfind)
        throw std::runtime_error(StringFormat("%s:%d:%s faield with %s",
                                              __FILE__, __LINE__, __FUNCTION__, data));
    *pfind = '\0';
    StringToNumber(data, &m_df, kValueBase);
    return pfind - data + strlen(kDelim);
}

