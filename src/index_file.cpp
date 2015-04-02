// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#include "src/index_file.h"

BaseIndexFile::BaseIndexFile(const std::string fname): m_fname(fname), m_max_termid(0), m_fd(NULL)
{
    MutexLocker locker(m_mutex);
    m_fd = fopen(fname.c_str(), "r");
    PCHECK(m_fd != NULL) << fname << " open failed";
}

bool BaseIndexFile::get_offset(offset_t *offset, const termid_t termid)
{
    termid2offset_t::const_iterator it = m_termid2offset.find(termid);
    if (it != m_termid2offset.end())
    {
        *offset = it->second;
        return true;
    }
    return false;
}

size_t BaseIndexFile::read(char *buff, size_t size, ssize_t offset, int whence)
{
    if (!m_fd)
        return 0;
    MutexLocker locker(m_mutex);
    if (fseek(m_fd, static_cast<long>(offset), whence))
        return 0;
    return fread(static_cast<void *>(buff), 1, size, m_fd);
}

void BaseIndexFile::dump_termlist(std::string *ret)
{
    for (termid2offset_t::const_iterator it = m_termid2offset.begin(); it != m_termid2offset.end(); ++it)
    {
        termid_t termid = it->first;
        // termid %= 1023;
        if (ret)
            StringFormatAppend(ret, "%lu\n", termid);
        else
            std::cout << StringFormat("%lu\n", termid);
    }
}

void TextIndexFile::init()
{
    static const ssize_t kMaxHeaderSize = 256;
    char header_buf[kMaxHeaderSize + 1] = {0};
    size_t nread = read(header_buf, kMaxHeaderSize, -kMaxHeaderSize, SEEK_END);
    if (nread == 0)
        throw std::runtime_error(StringFormat("%s:%d:%s %s open faield with %s\n", __FILE__, __LINE__, __FUNCTION__,
                                              get_fname().c_str(), strerror(errno)));
    StringPiece header(header_buf, nread);
    size_t pos = header.rfind("{");
    if (pos != std::string::npos)
        header = header.substr(pos, header.size() - pos);
    Json::Value header_map;
    Json::Reader reader;
    if (!reader.parse(header.data(),
                      header.data() + header.size(),
                      header_map)) {
        LOG(WARNING) << header << " illegal!";
        return;
    }
    size_t ioffset = header_map["ioffset"].asUInt();
    size_t ilen = header_map["ilen"].asUInt();
    size_t plen = header_map["plen"].asUInt();
    // decode term list
    scoped_array<char> termlist_buf(new char[ilen + 1]);
    nread = read(termlist_buf.get(), ilen, ioffset);
    termlist_buf[nread] = '\0';
    if (nread != ilen) {
        LOG(WARNING) << "termlist illegal ioffset=" << ioffset << ",ilen=" << ilen << ",nread=" << nread;
        return;
    }
    std::vector<termid_t> termlist;
    char *data = termlist_buf.get();
    char *pfind = strstr(data, kDelim);
    if (!pfind)
        throw std::runtime_error(StringFormat("%s:%d:%s faield with %s\n", __FILE__, __LINE__, __FUNCTION__, data));
    termid_t last_termid(0);
    while ( (pfind = strstr(data, kDelim)) != NULL )
    {
        *pfind = '\0';
        termid_t termid;
        StringToNumber(data, &termid, kValueBase);
        last_termid += termid;
        termlist.push_back(last_termid);
        data = pfind + strlen(kDelim);
    }
    m_max_termid = last_termid;

    // decode offset list
    std::vector<offset_t> offsetlist;
    offsetlist.reserve(termlist.size());
    scoped_array<char> offsetlist_buf(new char[plen + 1]);
    nread = read(offsetlist_buf.get(), plen, ioffset + nread);
    offsetlist_buf[nread] = '\0';
    if (nread != plen) {
        LOG(WARNING) << "offsetlist illegal ilen=" << plen << ",nread=" << nread;
        return;
    }
    data = offsetlist_buf.get();
    pfind = strstr(data, kDelim);
    if (!pfind)
        throw std::runtime_error(StringFormat("%s:%d:%s faield with %s\n", __FILE__, __LINE__, __FUNCTION__, data));
    int64_t last_offset(0);
    while( (pfind = strstr(data, kDelim)) != NULL )
    {
        *pfind = '\0';
        // offset offset不是升序排列的(termid是升序排列的), 由于做了差分压缩,所有可能是负数
        int64_t offset;
        StringToNumber(data, &offset, kValueBase);
        assert(last_offset + offset >= 0);
        last_offset += offset;
        offsetlist.push_back(static_cast<offset_t>(last_offset));
        data = pfind + strlen(kDelim);
    }
    if (termlist.size() != offsetlist.size())
        throw std::runtime_error(StringFormat("%s:%d:%s faield with %lu!=%lu\n", __FILE__, __LINE__,
                                              __FUNCTION__, termlist.size(), offsetlist.size()));


    // m_termid2offset
    for (size_t i = 0; i < termlist.size(); ++i)
    {
        const termid_t &termid = termlist[i];
        const offset_t &offset = offsetlist[i];
        m_termid2offset.insert(std::pair<termid_t, offset_t>(termid, offset));
    }
    LOG(INFO) << get_fname() << " load " << m_termid2offset.size() << " records.";
}


Index::Index(const std::string pattern)
{
    init(pattern);
}

int32_t Index::init(const std::string pattern)
{
    glob_t globbuf;
    globbuf.gl_offs = 0;
    glob(pattern.c_str(), GLOB_DOOFFS, NULL, &globbuf);
    std::vector<std::string> fnames;
    for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
        fnames.push_back(globbuf.gl_pathv[i]);
    }
    globfree(&globbuf);
    ThreadPool thread_pool(16, 32);
    for (size_t i = 0; i < fnames.size(); ++i) {
        const std::string& name = fnames[i];
        IIndexFile * indexfile = new_index_file(name, thread_pool);
        m_idxfiles.push_back(indexfile);
    }
    thread_pool.WaitForIdle();
    return 0;
}

IIndexFile * Index::new_index_file(const std::string name, ThreadPool& thread_pool)
{
    TextIndexFile * indexfile = new TextIndexFile(name);
    // 放在这里是迫不得已
    Closure<void>* closure = NewClosure(indexfile, &TextIndexFile::init);
    thread_pool.AddTask(closure);
    return indexfile;
}

bool Index::new_queryterm(IQueryTerm **queryterm, const termid_t termid)
{
    // @todo 按照termid进行hash, 但由于索引是mapreduce streaming 建的,hash函数不好控制
    for(std::vector<IIndexFile*>::const_iterator it = m_idxfiles.begin(); it != m_idxfiles.end(); ++it)
    {
        offset_t offset(0);
        if ( (*it)->get_offset(&offset, termid))
        {
            *queryterm = new TextQueryTerm(*it, termid, offset);
            return true;
        }
    }
    return false;
}

void Index::dump_termlist(std::string *ret)
{
    for(std::vector<IIndexFile*>::const_iterator it = m_idxfiles.begin(); it != m_idxfiles.end(); ++it)
    {
        (*it)->dump_termlist(ret);
    }
}
