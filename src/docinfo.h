#ifndef SRC_DOCINFO_H
#define SRC_DOCINFO_H
#pragma once
#include <algorithm>
#include <vector>
#include "std_def.h"

typedef class DocInfo
{
public:
    std::string to_string() const
    {
        return StringFormat("%x\t%x\t%x\t%x\t%x\t%x", pageid_, state_,
                            classid_, answer_cnt_, click_cnt_, rich_);
    }
    pageid_t pageid_;
    uint32_t state_;
    uint32_t classid_;
    uint16_t answer_cnt_;
    uint16_t click_cnt_;
    uint8_t rich_;
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
        for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
            const char *fname = globbuf.gl_pathv[i];
            FILE * fd = fopen(fname, "r");
            if (!fd)
            {
                PLOG(ERROR) << fname << " open failed";
                continue;
            }
            Closure<void>* closure = NewClosure(this, &DocInfoManager::load_fd, fd);
            thread_pool_.AddTask(closure);
        }
        thread_pool_.WaitForIdle();
        std::sort(infos_.begin(), infos_.end(), cmp);
        return infos_.size();
    }

    const docinfo_t * get_by_id(pageid_t pageid) const
    {
        ssize_t left(0), right(infos_.size());
        while (left <= right)
        {
            ssize_t mid = (left + right) / 2;
            const docinfo_t *curr = &infos_[mid];
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

    void load_fd(FILE *fd)
    {
        char * line = NULL;
        size_t len = 0;
        ssize_t read;
        if (!fd)
            return;
        std::vector<docinfo_t> docinfos;
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
            StringToNumber(pstate, &docinfo.state_, kValueBase);
            StringToNumber(pclassid, &docinfo.classid_, kValueBase);
            StringToNumber(panswer_cnt, &docinfo.answer_cnt_, kValueBase);
            StringToNumber(pclick_cnt, &docinfo.click_cnt_, kValueBase);
            StringToNumber(prich, &docinfo.rich_, kValueBase);
            docinfos.push_back(docinfo);
        }
        fclose(fd);
        if (line)
            free(line);
        MutexLocker locker(&mutex_);
        infos_.reserve(infos_.size() + docinfos.size());
        infos_.insert(infos_.begin(), docinfos.begin(), docinfos.end());
    }
private:
    std::vector<docinfo_t> infos_;
    ThreadPool thread_pool_;
    Mutex mutex_;
};
#endif // SRC_DOCINFO_H
