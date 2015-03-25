cc_library(
    name = 'index_file',
    srcs = [
        'index_file.cpp',
        'query_server.cpp',
        'query_term.cpp',
    ],
    deps = [
        '//common/base/string:string',
        '//common/system/concurrency:concurrency',
        '//thirdparty/jsoncpp:jsoncpp',
        '//thirdparty/glog:glog',
    ],
)

cc_binary(
    name = 'qs',
    srcs = [
        'query_server.cpp',
    ],
    deps = [
        '//common/base/string:string',
        '//common/net/http:http',
        '//common/crypto/hash:hash',
        '//common/encoding:encoding',
        '//common/system/concurrency:concurrency',
        '//src:index_file',
    ],
)

cc_binary(
    name = 'query_term_test',
    srcs = 'query_term_test.cpp',
    deps = [
        '//common/base/string:string',
        '//common/net/http:http',
        '//common/crypto/hash:hash',
        '//common/encoding:encoding',
        '//common/system/concurrency:concurrency',
        '//src:index_file',
    ],
)

cc_binary(
    name = 'index_file_test',
    srcs = 'index_file_test.cpp',
    deps = [
        '//common/base/string:string',
        '//common/net/http:http',
        '//common/crypto/hash:hash',
        '//common/encoding:encoding',
        '//common/system/concurrency:concurrency',
        '//src:index_file',
    ],
)

cc_binary(
    name = 'intersector_test',
    srcs = 'intersector_test.cpp',
    deps = [
        '//common/base/string:string',
        '//common/net/http:http',
        '//common/crypto/hash:hash',
        '//common/encoding:encoding',
        '//common/system/concurrency:concurrency',
        '//src:index_file',
    ],
)

cc_binary(
    name = 'dnf_expr_test',
    srcs = [
        'dnf_expr_test.cpp',
    ],
    deps = [
        '//common/base/string:string',
        '//common/net/http:http',
        '//common/crypto/hash:hash',
        '//common/encoding:encoding',
        '//common/system/concurrency:concurrency',
        '//src:index_file',
    ],
)

cc_binary(
    name = 'docinfo_test',
    srcs = [
        'docinfo_test.cpp',
    ],
    deps = [
        '//common/base/string:string',
        '//common/net/http:http',
        '//common/crypto/hash:hash',
        '//common/encoding:encoding',
        '//common/system/concurrency:concurrency',
        '//src:index_file',
    ],
)
