#include "thirdparty/gtest/gtest.h"
#include "docinfo.h"

int main(int argc, char **argv)
{
    DocInfos infos;
    std::cout << "load " << infos.load(argv[1]) << " records." << std::endl;
    const docinfo_t *docinfo = infos.get_by_id(268448835);
    if (docinfo)
        std::cout << docinfo->to_string() << std::endl;
    return 0;
}
