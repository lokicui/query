#ifndef SRC_DOCINFO_H
#define SRC_DOCINFO_H
#pragma once
#include <algorithm>
#include <list>
#include <vector>
#include "std_def.h"

typedef class DocInfo
{
public:
    std::string to_string() const
    {
        return StringFormat("%x\t%x\t%x\t%x\t%x\t%x", pageid_, resolved_,
                            classid_, answer_cnt_, click_cnt_, rich_);
    }
    uint32_t pageid_;
    uint32_t classid_;
    uint16_t answer_cnt_;
    uint16_t click_cnt_;
    uint8_t resolved_ : 1;
    uint8_t elite_ : 1;
    uint8_t rich_ : 1;
}docinfo_t;

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
        std::list<docinfo_t> docinfolist;
        ThreadPool thread_pool(16, 32);
        for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
            const char *fname = globbuf.gl_pathv[i];
            FILE * fd = fopen(fname, "r");
            if (!fd)
            {
                PLOG(ERROR) << fname << " open failed";
                continue;
            }
            Closure<void>* closure = NewClosure(this, &DocInfoManager::load_fd, &docinfolist, fname, fd);
            thread_pool.AddTask(closure);
        }
        thread_pool.WaitForIdle();
        globfree(&globbuf);
        docinfolist_.reserve(docinfolist.size());
        docinfolist_.assign(docinfolist.begin(), docinfolist.end());
        LOG(INFO) << "load " << docinfolist_.size() << " finish!";
        std::sort(docinfolist_.begin(), docinfolist_.end(), cmp);
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
    static bool cmp(const docinfo_t& lhs, const docinfo_t& rhs)
    {
        return lhs.pageid_ < rhs.pageid_;
    }

    void load_fd(std::list<docinfo_t> *docinfolist, const char *fname, FILE *fd)
    {
        char * line = NULL;
        size_t len = 0;
        ssize_t read;
        if (!fd)
            return;
        std::list<docinfo_t> docinfos;
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
            if (items.size() < 6)
                continue;
            const char* pqid = items[0];
            const char* pstate = items[1];
            const char* pclassid = items[2];
            const char* panswer_cnt = items[3];
            const char* pclick_cnt = items[4];
            const char* prich = items[5];
            docinfo_t docinfo;
            StringToNumber(pqid, &docinfo.pageid_, kValueBase);
            uint32_t state(0), rich(0);
            StringToNumber(pstate, &state, kValueBase);
            StringToNumber(pclassid, &docinfo.classid_, kValueBase);
            StringToNumber(panswer_cnt, &docinfo.answer_cnt_, kValueBase);
            StringToNumber(pclick_cnt, &docinfo.click_cnt_, kValueBase);
            StringToNumber(prich, &rich, kValueBase);
            docinfo.rich_ = bool(rich);
            docinfo.elite_ = (docinfo.pageid_ > 2000000000);
            docinfo.resolved_ = state == 4;
            docinfos.push_back(docinfo);
        }
        fclose(fd);
        if (line)
            free(line);
        LOG(INFO) << fname << " load " << docinfos.size() << " records.";
        MutexLocker locker(&mutex_);
        docinfolist->insert(docinfolist->end(), docinfos.begin(), docinfos.end());
    }
private:
    std::vector<docinfo_t> docinfolist_;
    Mutex mutex_;
};
#endif // SRC_DOCINFO_H
