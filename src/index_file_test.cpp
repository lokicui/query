// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#include "src/index_file.h"
#include "src/query_term.h"
#include "thirdparty/gtest/gtest.h"
int main(int argc, char **argv)
{
    // IndexFile * indexfile = new TextIndexFile(argv[1]);
    // indexfile->init();
    // std::cout << indexfile->dump_termlist() << std::endl;

    Index *index = new Index(argv[1]);
    index->dump_termlist();
    delete index;
    return 0;
}
