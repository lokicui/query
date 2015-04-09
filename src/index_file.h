// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#ifndef SRC_INDEX_FILE_H
#define SRC_INDEX_FILE_H
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "src/query_term.h"
#include "src/std_def.h"

class IQueryTerm;

// Interface of index file
class IIndexFile
{
public:
    virtual ~IIndexFile() {}
    virtual void init() = 0;
    virtual size_t read(char *buff, size_t size, ssize_t offset, int whence = SEEK_SET) = 0;
    virtual bool get_offset(offset_t *offset, const termid_t termid) = 0;
    virtual void dump_termlist(std::string *ret = NULL) = 0;
    virtual termid_t get_max_termid() const = 0;
    virtual std::string get_fname() const = 0;
};

// 实现一些基本函数, TextIndexFile BinaryIndexFile共有的函数都放这里了
// 但base类仍然有一些接口没有实现,还是个抽象类,所以不能生成Base类的对象
class BaseIndexFile: public IIndexFile
{
public:
    typedef std::pair<termid_t, offset_t> termid2offset_t;
    typedef std::vector<termid2offset_t> termid2offset_array_t;
public:
    explicit BaseIndexFile(const std::string fname);
    // virtual void init() = 0;
    size_t read(char *buff, size_t size, ssize_t offset, int whence = SEEK_SET);
    bool get_offset(offset_t *offset, const termid_t termid);
    void dump_termlist(std::string *ret = NULL);
    ~BaseIndexFile()
    {
        if (m_fd)
        {
            fclose(m_fd);
            m_fd = NULL;
        }
    }
    termid_t get_max_termid() const
    {
        return m_max_termid;
    }
    std::string get_fname() const
    {
        return m_fname;
    }
protected:
    static bool cmp_(const termid2offset_t &lhs, const termid2offset_t &rhs)
    {
        return lhs.first < rhs.first;
    }
    size_t read_(char *buff, size_t size, long offset);
protected:
    const std::string m_fname;
    termid_t m_max_termid;
    Mutex m_mutex;
    FILE* m_fd;
    termid2offset_array_t m_termid2offset;
};

// 纯文本的索引, hadoop streaming生成的
class TextIndexFile: public BaseIndexFile
{
public:
    explicit TextIndexFile(const std::string fname) : BaseIndexFile(fname) {}
    ~TextIndexFile() {}
    void init();
};

// 二进制索引, index_merger 生成,通过TextIndexFile生成BinaryIndexFile
class BinaryIndexFile: public BaseIndexFile
{
public:
    explicit BinaryIndexFile(const std::string fname) : BaseIndexFile(fname) {}
    ~BinaryIndexFile() {}
    void init();
};

class Index
{
public:
    Index() {}
    explicit Index(const std::string pattern);
    int32_t init(const std::string pattern);
    void dump_termlist(std::string *ret = NULL);
    bool new_queryterm(IQueryTerm **queryterm, const termid_t termid);
private:
    IIndexFile * new_index_file(const std::string name, ThreadPool *thread_pool);
private:
    std::vector<IIndexFile*> m_idxfiles;
};

#endif // SRC_INDEX_FILE_H
