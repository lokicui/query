// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#ifndef SRC_DOCINFO_H
#define SRC_DOCINFO_H
#pragma once
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include "src/std_def.h"

#pragma pack(push)
#pragma pack(1)
typedef struct DocInfo
{
    DocInfo():pageid_(0), classid_(0), time_(0), selected_(0), adoptrate_(0)
    {}
    std::string to_string() const
    {
        return StringFormat("%" PRIx64 "\t%x\t%x\t%x", pageid_, classid_, selected_, adoptrate_);
    }
    pageid_t pageid_;
    uint32_t classid_;
    uint32_t time_;
    uint8_t selected_ : 1;
    uint8_t adoptrate_ : 7;
}docinfo_t;
#pragma pack(pop)

class DocInfoManager
{
public:
    DocInfoManager()
    {
    }
    size_t load(const std::string pattern)
    {
        glob_t globbuf;
        globbuf.gl_offs = 0;
        glob(pattern.c_str(), GLOB_DOOFFS, NULL, &globbuf);
        std::vector< std::vector<docinfo_t>* > docinfolist;
        ThreadPool thread_pool(16, 32);
        // 计算出record size
        for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
            const char *fname = globbuf.gl_pathv[i];
            FILE * fd = fopen(fname, "r");
            if (!fd)
            {
                PLOG(ERROR) << fname << " open failed";
                continue;
            }
            docinfolist.push_back(new std::vector<docinfo_t>());
            // fd 在load_fd函数中被close
            Closure<void>* closure = NewClosure(this, &DocInfoManager::load_fd,
                                               docinfolist.back(), fname, fd);
            thread_pool.AddTask(closure);
        }
        thread_pool.WaitForIdle();
        size_t total(0);
        for (std::vector< std::vector<docinfo_t>* >::const_iterator it = docinfolist.begin();
             it != docinfolist.end(); ++it)
        {
            total += (*it)->size();
            delete (*it);
        }

        // 先预分配内存
        docinfolist_.reserve(total);
        LOG(INFO) << "prepare to load " << total << " records.";
        //正式加载到内存
        for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
            const char *fname = globbuf.gl_pathv[i];
            FILE * fd = fopen(fname, "r");
            if (!fd)
            {
                PLOG(ERROR) << fname << " open failed";
                continue;
            }
            docinfolist.push_back(new std::vector<docinfo_t>());
            // fd 在load_fd函数中被close
            Closure<void>* closure = NewClosure(this, &DocInfoManager::load_fd,
                                               &docinfolist_, fname, fd);
            thread_pool.AddTask(closure);
        }
        thread_pool.WaitForIdle();
        globfree(&globbuf);
        LOG(INFO) << "load " << docinfolist_.size() << " finish,capacity=" <<
            docinfolist_.capacity();
        std::sort(docinfolist_.begin(), docinfolist_.end(), cmp_);
        LOG(INFO) << "sort " << docinfolist_.size() << " finish!";
        return docinfolist_.size();
    }

    const docinfo_t * get_by_id(pageid_t pageid) const
    {
        ssize_t left(0), right(docinfolist_.size());
        if (docinfolist_.empty())
            return NULL;
        while (left <= right)
        {
            ssize_t mid = (left + right) / 2;
            const docinfo_t *curr = &docinfolist_[mid];
            if (pageid == curr->pageid_)
                return curr;
            else if (pageid < curr->pageid_)
                right = mid - 1;
            else if (pageid > curr->pageid_)
                left = mid + 1;
        }
        return NULL;
    }

private:
    static bool cmp_(const docinfo_t& lhs, const docinfo_t& rhs)
    {
        return lhs.pageid_ < rhs.pageid_;
    }

    void load_fd(std::vector<docinfo_t> *docinfos, const char *fname, FILE *fd)
    {
        char * line = NULL;
        size_t len = 0;
        ssize_t read;
        if (!fd)
            return;
        std::vector<docinfo_t> docinfolist_tmp;
        static const size_t kBufSize = 64 << 20; // 64M
        scoped_array<char> buf(new char[kBufSize]);
        setbuffer(fd, buf.get(), kBufSize);
        while ((read = getline(&line, &len, fd)) != -1)
        {
            line[read] = '\0';
            char *start = line;
            char *end = line;
            std::vector<const char*> items;
            while (*end != '\n' && *end != '\0')
            {
                if (*end == '\t')
                {
                    *end = '\0';
                    items.push_back(start);
                    start = ++end;
                }
                else
                {
                    ++end;
                }
            }
            items.push_back(start);
            if (items.size() < 5)
                continue;
            const char* ppageid = items[0];
            const char* pclassid = items[1];
            const char* pselected = items[2];
            const char* padoptrate = items[3];
            const char* ptime = items[4];
            docinfo_t docinfo;
            char *endptr(NULL);
            docinfo.pageid_ = strtoull(ppageid, &endptr, kValueBase);
            docinfo.classid_ = strtoull(pclassid, &endptr, kValueBase);
            docinfo.selected_ = strtoull(pselected, &endptr, kValueBase);
            docinfo.adoptrate_ = strtoull(padoptrate, &endptr, kValueBase);
            docinfo.time_ = strtoull(ptime, &endptr, kValueBase);
            docinfolist_tmp.push_back(docinfo);
            // std::cout << docinfo.to_string() << std::endl;
        }
        fclose(fd);
        if (line)
            free(line);
        std::sort(docinfolist_tmp.begin(), docinfolist_tmp.end(), cmp_);
        MutexLocker locker(&mutex_);
        docinfos->insert(docinfos->end(), docinfolist_tmp.begin(), docinfolist_tmp.end());
        LOG(INFO) << fname << " load " << docinfos->size() << " records.";
    }
private:
    std::vector<docinfo_t> docinfolist_;
    Mutex mutex_;
};
#endif // SRC_DOCINFO_H
