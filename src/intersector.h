#ifndef SRC_INTERSECTOR_H
#define SRC_INTERSECTOR_H
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <vector>
#include "common/base/scoped_ptr.h"
#include "common/base/string/algorithm.h"
#include "common/base/string/format.h"
#include "common/base/string/string_number.h"
#include "common/base/string/string_piece.h"
#include "common/system/concurrency/mutex.h"
#include "src/query_term.h"
#include "src/std_def.h"

class LoserTree
{
    // 暂不用败者树实现
};

class QueryTerm;
class Intersector
{
public:
    typedef std::vector<QueryTerm*> querytermlist_t;
public:
    Intersector(const querytermlist_t &termlist)
    {
        // @todo query term排重
        querytermlist_t termlist_sort;
        termlist_sort.assign(termlist.begin(), termlist.end());
        if (!termlist_sort.empty())
        {
            std::sort(termlist_sort.begin(), termlist_sort.end(), query_term_cmp);
            m_min_df_term = termlist_sort[0];
            m_qtlist.assign(termlist_sort.begin() + 1, termlist_sort.end());
        }
    }
    bool next(pageid_t *doc, const pageid_t predoc)
    {
        QueryTerm * min_df_term = get_min_df_term();
        querytermlist_t * qtlist = get_other_qtlist();
        if (!min_df_term || !qtlist)
            return false;
        pageid_t kdoc(0), cdoc(0);
        querytermlist_t::const_iterator it;
        // qtlist按其df升序排, qtlist中没有min_df_term
        if (!min_df_term->seek_lower_bound(&kdoc, predoc + 1))
            return false;
again:
        for (it = qtlist->begin(); it != qtlist->end(); ++ it)
        {
            if (!(*it)->seek_lower_bound(&cdoc, kdoc)) // nothing more
                return false;
            assert(cdoc >= kdoc);
            // no hit
            if (cdoc != kdoc)
            {
                if (!min_df_term->seek_lower_bound(&kdoc, cdoc))
                    return false;
                goto again;
            }
        }
        *doc = kdoc;
        return true;
    }

private:
    static bool query_term_cmp(const QueryTerm* lhs, const QueryTerm *rhs)
    {
        return lhs->get_df() < rhs->get_df();
    }
    querytermlist_t * get_other_qtlist()
    {
        return &m_qtlist;
    }
    QueryTerm* get_min_df_term()
    {
        return m_min_df_term;
    }

private:
    querytermlist_t m_qtlist;
   QueryTerm *m_min_df_term;
};

#endif // SRC_INTERSECTOR_H
