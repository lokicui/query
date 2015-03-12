#include <iostream>
#include "src/index_file.h"
#include "src/intersector.h"
#include "thirdparty/gtest/gtest.h"

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

    Intersector *intersector = new Intersector(terms);
    pageid_t pageid(0);
    while (intersector->next(&pageid, pageid))
    {
        std::cout << pageid << std::endl;
    }
    return 0;
}
