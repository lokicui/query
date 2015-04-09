// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#include "thirdparty/gtest/gtest.h"
#include "docinfo.h"

int main(int argc, char **argv)
{
    // std::cout << sizeof(docinfo_t) << std::endl;
    DocInfoManager infos;
    std::cout << "load " << infos.load(argv[1]) << " records." << std::endl;
    const docinfo_t *docinfo = infos.get_by_id(0x1000007737709280790);
    if (docinfo)
        std::cout << docinfo->to_string() << std::endl;
    std::cout << "----------------------------------" << std::endl;
    sleep(15);
    return 0;
}
