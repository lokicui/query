// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#ifndef SRC_STD_DEF_H
#define SRC_STD_DEF_H
#pragma once
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "common/base/closure.h"
#include "common/base/compatible/string.h"
#include "common/base/scoped_ptr.h"
#include "common/base/string/algorithm.h"
#include "common/base/string/string_number.h"
#include "common/base/string/string_piece.h"
#include "common/crypto/hash/md5.h"
#include "common/encoding/charset_converter.h"
#include "common/net/http/http_client.h"
#include "common/net/http/http_server.h"
#include "common/netframe/netframe.h"
#include "common/system/concurrency/atomic/atomic.hpp"
#include "common/system/concurrency/base_thread.h"
#include "common/system/concurrency/mutex.h"
#include "common/system/concurrency/this_thread.h"
#include "common/system/concurrency/thread_pool.h"
#include "common/system/system_information.h"
#include "thirdparty/gflags/gflags.h"
#include "thirdparty/glog/logging.h"
#include "thirdparty/jsoncpp/json.h"
#include "thirdparty/jsoncpp/reader.h"
#include "thirdparty/jsoncpp/value.h"
#include "thirdparty/jsoncpp/writer.h"

typedef uint32_t offset_t;
typedef uint64_t termid_t;
typedef uint64_t docid_t;
typedef uint64_t pageid_t;


static const uint32_t kValueBase = 16;      // docid采用几进制进行存储的, 适用于文本存储的索引
static const char* kDelim = ",";

static docid_t pageid2docid(pageid_t pageid)
{
    // 根据pageid自增来建索引可以显著的减少索引文件大小,索引建完后生成一个pageid->docid的table
    // query_server加载该table来进行pageid到docid的转换
    return docid_t(pageid);
}

#endif // SRC_STD_DEF_H
