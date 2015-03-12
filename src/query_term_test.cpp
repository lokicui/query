// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#include "thirdparty/gtest/gtest.h"
#include "common/base/scoped_ptr.h"
#include "std_def.h"
#include "index_file.h"
#include "query_term.h"

int main(int argc, char **argv)
{
    IndexFile *fd = new TextIndexFile(argv[1]);
    offset_t offset = atoi(argv[2]);
    QueryTerm *qt = new QueryTerm(fd, offset, 1);
    pageid_t pageid;
    while (true)
    {
        if (!qt->next(&pageid))
            break;
        std::cout << pageid << std::endl;
        pageid_t bound;
        if (!qt->seek_lower_bound(&bound, pageid))
            break;
        assert(bound == pageid);
    }
    delete fd;
    delete qt;
    return 0;
};
