// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#include "thirdparty/gtest/gtest.h"
#include "common/base/scoped_ptr.h"
#include <vector>
#include "src/dnf_expr.h"
#include "src/query_term.h"

int main(int argc, char **argv)
{
    Index *index = new Index(argv[1]);
    std::vector<termid_t> termids;
    termids.push_back(1);
    termids.push_back(2);
    termids.push_back(3);
    termids.push_back(4);
    termids.push_back(5);
    std::vector<QueryTerm*> terms;
    for (std::vector<termid_t>::const_iterator it = termids.begin(); it != termids.end(); ++it)
    {
        QueryTerm *qt;
        if (index->new_queryterm(&qt, *it))
            terms.push_back(qt);
    }
    typedef DNFExpr<QueryTerm*> dnf_t;
    typedef dnf_t::candidate_t candidate_t;

    candidate_t candidate1, candidate2;
    candidate1.assign(terms.begin(), terms.end());
    terms.pop_back();
    candidate2.assign(terms.begin(), terms.end());

    dnf_t  expr;
    expr.add(&candidate1);
    expr.add(&candidate2);
    pageid_t docid(0);
    std::vector< const dnf_t::candidate_t * > match_candidates;
    while (expr.next(&docid, &match_candidates, docid))
    {
        std::cout << docid << std::endl;
    }

    for (std::vector<QueryTerm*>::const_iterator it = terms.begin(); it != terms.end(); ++it)
    {
        delete *it;
    }
    delete index;
    return 0;
}
