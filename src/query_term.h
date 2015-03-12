// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#ifndef SRC_QUERY_TERM_H
#define SRC_QUERY_TERM_H
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <utility>
#include <vector>
#include "src/index_file.h"
#include "src/std_def.h"
/*
 * 若term数量过多，每次调用的时候new出来，用完之后销毁
 * 若term数量较少，可以常驻内存
 *
 */

class IndexFile;
class QueryTerm
{
public:
    typedef std::vector<pageid_t> pagelist_t;
    typedef std::pair<pageid_t, offset_t> skipair_t;
    typedef std::vector<skipair_t> skiplist_t;
    static const size_t kCacheSize = 10 * 1024; // 最多缓存多少个pagelist在内存中
    static const size_t kHeaderSize = 4096;   // 设置为Header的平均长度的下限,包括skiplist的大小

public:
    QueryTerm(IndexFile* fd, termid_t termid, offset_t o);
    bool next(pageid_t *pageid);
    bool seek_lower_bound(pageid_t* bound, const pageid_t pageid);  // >=
    bool init();
    uint32_t get_df() const
    {
        return m_df;
    }
    termid_t get_termid() const
    {
        return m_termid;
    }
    int32_t get_block_index() const
    {
        return m_block_idx;
    }
    void set_block_index(int32_t block_index)
    {
        m_block_idx = block_index;
    }

private:
    size_t read_next_block();
    size_t read_block(int32_t block_index);
    offset_t get_block_cursor(int32_t block_index);
    size_t get_block_size(int32_t block_index);
    size_t decode_skiplist(char *data);
    size_t decode_block_pagelist(char *data);
    size_t decode_df(char *data);
    static bool skipair_cmp_function(const skipair_t& lhs, const skipair_t& rhs)
    {
        return lhs.first < rhs.first;
    }

private:
    IndexFile *m_fd;
    const offset_t m_offset;       // terminfo在m_fd中的偏移, const
    offset_t m_cursor;             // doclist 第一个block在文件中的偏移,skiplist中的offset以此为基址
    uint32_t m_df;                 // term的df,总共有多少doc
    int32_t m_block_idx;           // 第m_block_idx 个block已经加载到内存中了
    size_t m_pagelist_idx;         // pagelist 中的第m_pagelist_idx 个元素已经访问过了
    termid_t m_termid;
    pagelist_t m_pagelist;         // pagelist buff
    skiplist_t m_skiplist;
    // skiplist 跳表,记录每个block的max pageid以及其在文件中的偏移,每个block需要单独解压
};
#endif // SRC_QUERY_TERM_H
