// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#ifndef SRC_INDEX_FILE_H
#define SRC_INDEX_FILE_H
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "src/query_term.h"
#include "src/std_def.h"

class QueryTerm;
class IndexFile
{
public:
    virtual ~IndexFile() {}
    virtual std::string get_fname() const = 0;
    virtual termid_t get_max_termid() const = 0;
    virtual size_t read(char *buff, size_t size, ssize_t offset, int whence = SEEK_SET) = 0;
    virtual bool get_offset(offset_t *offset, const termid_t termid) = 0;
    virtual void init() = 0;
    virtual void dump_termlist(std::string *ret = NULL) = 0;
};

class TextIndexFile: public IndexFile
{
public:
    typedef std::map<termid_t, offset_t> termid2offset_t;
public:
    explicit TextIndexFile(const std::string fname);
    ~TextIndexFile() { if (m_fd) fclose(m_fd); }
    bool get_offset(offset_t *offset, const termid_t termid);
    void dump_termlist(std::string *ret = NULL);
    void init();
    size_t read(char *buff, size_t size, ssize_t offset, int whence = SEEK_SET);
    termid_t get_max_termid() const
    {
        return m_max_termid;
    }
    std::string get_fname() const
    {
        return m_fname;
    }

private:
    size_t read_(char *buff, size_t size, long offset);
private:
    const std::string m_fname;
    termid_t m_max_termid;
    Mutex m_mutex;
    FILE* m_fd;
    termid2offset_t m_termid2offset;
};

class Index
{
public:
    Index() {}
    explicit Index(const std::string pattern);
    int32_t init(const std::string pattern);
    void dump_termlist(std::string *ret = NULL);
    bool new_queryterm(QueryTerm **queryterm, const termid_t termid);
private:
    IndexFile * new_index_file(const std::string name);
private:
    ThreadPool m_thread_pool;
    std::vector<IndexFile*> m_idxfiles;
};

#endif // SRC_INDEX_FILE_H
