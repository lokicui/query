#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <glob.h>
#include "thirdparty/jsoncpp/json.h"
#include "thirdparty/jsoncpp/reader.h"
#include "thirdparty/jsoncpp/value.h"
#include "thirdparty/jsoncpp/writer.h"
#include "common/base/closure.h"
#include "common/base/compatible/string.h"
#include "common/base/string/string_number.h"
#include "common/base/string/string_piece.h"
#include "common/base/string/algorithm.h"
#include "common/base/string/algorithm.h"
#include "common/crypto/hash/md5.h"
#include "common/encoding/charset_converter.h"
#include "common/net/http/http_client.h"
#include "common/net/http/http_server.h"
#include "common/netframe/netframe.h"
#include "common/system/concurrency/atomic/atomic.hpp"
#include "common/system/concurrency/base_thread.h"
#include "common/system/concurrency/this_thread.h"
#include "common/system/concurrency/thread_pool.h"
#include "thirdparty/gflags/gflags.h"
#include "thirdparty/glog/logging.h"
#include "src/docinfo.h"
#include "src/index_file.h"
#include "src/intersector.h"
#include "src/std_def.h"

using namespace std;

DEFINE_int32(request_number, 10, "max request number");
DEFINE_int32(request_tos, 96, "default tos value");
DEFINE_int32(max_gram_num, 5, "max gram number");
DEFINE_int32(listen_port, 12345, "default listen port");
DEFINE_string(index_data, "../index_data", "default index file path");
DEFINE_string(docinfo_data, "../docinfo_data", "default docinfo file path");

Atomic<bool> g_server_started = false;
ThreadPool g_thread_pool;
Index * g_index(NULL);
DocInfoManager *g_docinfo_mgr(NULL);
const std::string g_get_handler_path = "/sget";
const std::string g_post_handler_path = "/spost";
map<string, string> g_stopwords_map;
SocketAddressStorage g_server_address;

int32_t init_stopwords(const char *name = "stopwords")
{
    char buff[512] = {0};
    snprintf(buff, sizeof(buff), "%s/%s", FLAGS_index_data.c_str(), name);
    FILE *fp = fopen(buff, "r");
    if (!fp)
        return -1;
    char *line(NULL);
    size_t len(0);
    ssize_t read(-1);
    while((read = getline(&line, &len, fp)) != -1)
    {
        RemoveLineEnding(line);
        if (strlen(line) == 0)
            continue;
        if (strlen(line) > 1 && line[0] == '#' && line[1] != '#')
            continue;
        g_stopwords_map[line] = "";
    }
    if (g_stopwords_map.empty())
        LOG(WARNING) << "stopwords dict empty!";
    return 0;
}

string get_normalized_kwds(string in)
{
    for(map<string, string>::const_iterator it = g_stopwords_map.begin(); it != g_stopwords_map.end(); ++ it)
    {
        const string& search = it->first;
        const string& replace = it->second;
        if (search.empty())
            continue;
        ReplaceAll(&in, search, replace);
    }
    return in;
}


int char2num(char ch)
{
    if(ch >= '0' && ch <= '9')
        return (ch - '0');
    if(ch >= 'a' && ch <= 'f')
        return (ch- 'a' + 10);
    if(ch >= 'A' && ch <= 'F')
        return (ch- 'A' + 10);
    return -1;
}


void urldecode(const std::string& input, std::string& output)
{
    output.clear();
    size_t i = 0;
    size_t len = input.size();
    int num1, num2;
    while(i < len)
    {
        char ch = input[i];
        switch(ch)
        {
        case '+':
            output.append(1, ' ');
            i++;
            break;
        case '%':
            if(i+2 < len)
            {
                num1 = char2num(input[i+1]);
                num2 = char2num(input[i+2]);
                if(num1 != -1 && num2 != -1)
                {
                    char res = (char)((num1 << 4) | num2);
                    output.append(1, res);
                    i += 3;
                    break;
                }
            }
            //go through
        default:
            output.append(1, ch);
            i++;
            break;
        }
    }
}

bool UTF16_To_UTF8(const string& in, string* out, size_t *converted_size)
{
    CharsetConverter c("UTF-16LE", "UTF-8");
    return c.Convert(in, out, converted_size);
}

bool UTF8_To_UTF16(const string& in, string* out, size_t *converted_size)
{
    CharsetConverter c("UTF-8", "UTF-16LE");
    return c.Convert(in, out, converted_size);
}



bool Split2Gram(const string& in, set<string> *ret, uint32_t max_gram_num = 5)
{
    // 转utf16进行截断,然后再转回来
    string converted;
    size_t converted_size(0);
    if (!UTF8_To_UTF16(in, &converted, &converted_size))
        return false;
    size_t csize = converted.size() / 2;
    if (csize <= max_gram_num)
    {
        ret->insert(in);
    }
    else
    {
        size_t num = (csize+max_gram_num-1) / max_gram_num;
        for (size_t i = 0; i < num; ++i)
        {
            size_t start = i * max_gram_num * 2;
            size_t end = (i+1) * max_gram_num * 2;
            if (end > converted.size())
                end = converted.size();
            if (end - start < 2 * 2 && start >= end - start)
                start -= end - start;
            string kd = converted.substr(start, end-start);
            string ckd;
            size_t size(0);
            if (UTF16_To_UTF8(kd, &ckd, &size))
            {
                ret->insert(ckd);
            }
        }
    }
    return true;
}

void DoProcessRequest(const HttpRequest* http_request,
        HttpResponse* http_response,
        Closure<void>* done)
{
    string url = http_request->HeadersToString();
    const string end_sign = " HTTP";
    size_t pos = url.find(end_sign);
    if (pos != string::npos)
    {
        url = url.substr(0, pos);
    }
    string kwds, kwds_decoded;
    const string start_sign = g_get_handler_path + "?kwds=";
    pos = url.find(start_sign);
    if (pos != string::npos)
    {
        pos += start_sign.length();
        size_t end = url.find("&", pos);
        if (end != string::npos)
            kwds = url.substr(pos, end);
        else
            kwds = url.substr(pos);
    }
    // url decode
    urldecode(kwds, kwds_decoded);
    // charset to utf8
    CharsetConverter gbk_to_utf8("GBK", "UTF-8");
    std::string converted, md5;
    size_t converted_size(0);
    if (false && gbk_to_utf8.Convert(kwds_decoded, &converted, &converted_size) && converted_size == kwds_decoded.length())
        kwds_decoded = converted;

    // log
    // std::cerr << http_request->HeadersToString();
    // std::cerr << kwds_decoded << "-->";
    if (!http_request->http_body().empty()) {
        // std::cerr << http_request->http_body();
    }

    Json::Value array_kwds;
    set<string> kwds_set;
    set<termid_t> termid_set;
    ReplaceAll(&kwds_decoded, "，", ",");
    ReplaceAll(&kwds_decoded, " ", ",");
    ReplaceAll(&kwds_decoded, "　", ",");
    SplitStringToSet(kwds_decoded, ",", &kwds_set);
    std::string kwds_pieces;
    for (set<string>::const_iterator it = kwds_set.begin(); it != kwds_set.end(); ++ it) {
        const std::string normalized_kwds = get_normalized_kwds(*it);
        array_kwds.append(normalized_kwds);
        set<string> kwds_;
        if (!Split2Gram(normalized_kwds, &kwds_, FLAGS_max_gram_num))
        {
            LOG(WARNING) << "Split2Gram failed";
            continue;
        }
        for (set<string>::const_iterator its = kwds_.begin(); its != kwds_.end(); ++ its)
        {
            const string& kwd = *its;
            kwds_pieces += kwd + "/";
            md5 = common::MD5::HexDigest(kwd);
            md5 = md5.substr(md5.length() - 16);
            termid_t termid;
            StringToNumber(md5, &termid, 16);
            LOG(INFO) << kwd << "->" << termid;
            termid_set.insert(termid);
        }
    }
    LOG(INFO) << kwds_decoded << "-->" << kwds_pieces;

    bool hitall(true);
    std::vector<IQueryTerm*> queryterms;
    for (std::set<termid_t>::const_iterator it = termid_set.begin(); it != termid_set.end(); ++it)
    {
        IQueryTerm *qt;
        if (g_index->new_queryterm(&qt, *it) && qt)
            queryterms.push_back(qt);
        else
        {
            LOG(WARNING) << *it << "dones't exists";
            hitall = false;
            break;
        }
    }

    static const size_t size_threshold = 1000;
    static const size_t statistics_threshold = 100000;
    uint32_t total_ans_cnt(0);
    uint32_t total_click_cnt(0);
    uint32_t have_click_cnt(0);
    uint32_t resolved_cnt(0);
    uint32_t elite_cnt(0);
    uint32_t rich_cnt(0);
    Json::Value array_docid;
    std::map<uint32_t, size_t> classid_st;
    size_t cnt(0);
    if (hitall)
    {
        Intersector intersector(queryterms);
        pageid_t pageid(0);
        while ( cnt < statistics_threshold && intersector.next(&pageid, pageid))
        {
            cnt ++;
            docid_t docid = pageid2docid(pageid);
            const docinfo_t *docinfo = g_docinfo_mgr->get_by_id(pageid);
            if (docinfo)
            {
                if (classid_st.find(docinfo->classid_) == classid_st.end())
                    classid_st[docinfo->classid_] = 0;
                classid_st[docinfo->classid_] ++;
                total_ans_cnt += docinfo->answer_cnt_;
                total_click_cnt += docinfo->click_cnt_;
                resolved_cnt += docinfo->resolved_;
                elite_cnt += docinfo->elite_;
                rich_cnt += docinfo->rich_;
            }
            if (array_docid.size() < size_threshold)
                array_docid.append(Json::UInt64(docid));
        }
    }

    for (std::vector<IQueryTerm*>::const_iterator it = queryterms.begin(); it != queryterms.end(); ++ it)
    {
        delete *it;
    }

    Json::Value class_st_array;
    for (std::map<uint32_t, size_t>::const_iterator it = classid_st.begin(); it != classid_st.end(); ++ it)
    {
        class_st_array[StringFormat("%u", it->first)] = Json::UInt64(it->second);
    }

    Json::Value root;
    Json::Value statistics;
    statistics["class"] = class_st_array;
    statistics["st_cnt"] = Json::UInt(cnt);
    statistics["total_ans_cnt"] = Json::UInt(total_ans_cnt);
    statistics["total_click_cnt"] = Json::UInt(total_click_cnt);
    statistics["rich_cnt"] = Json::UInt(rich_cnt);
    statistics["elite_cnt"] = Json::UInt(elite_cnt);
    statistics["resolved_cnt"] = Json::UInt(resolved_cnt);
    root["statistics"] = statistics;
    root["docids"] = array_docid;
    root["kwds"] = array_kwds;
    Json::FastWriter writer;
    http_response->AddHeader("Content-Type", "charset=utf-8");
    http_response->mutable_http_body()->append(writer.write(root));
    done->Run();
}


void AsyncProcessRequest(const HttpRequest* http_request,
        HttpResponse* http_response,
        Closure<void>* done)
{
    Closure<void>* closure = NewClosure(DoProcessRequest,
            http_request, http_response, done);
    g_thread_pool.AddTask(closure);
}

class HttpServerThread : public BaseThread
{
public:
    void Entry()
    {
        netframe::NetFrame net_frame;
        // Two http servers share a netframe.
        HttpServer http_server(&net_frame, false);
        HttpClosure* closure = NewPermanentClosure(DoProcessRequest);
        SimpleHttpServerHandler* handler = new SimpleHttpServerHandler(closure);
        http_server.RegisterHandler(g_get_handler_path, handler);
        http_server.RegisterHandler(g_post_handler_path, handler);

        // Start the two servers.
        bool server_started = http_server.Start(StringFormat(":%u", FLAGS_listen_port), &g_server_address);
        if (server_started) {
            g_server_started = true;
        }

        while (!IsStopRequested()) {
            ThisThread::Sleep(1000);
        }
        g_thread_pool.WaitForIdle();
        http_server.Stop();
    }
};


int main(int argc, char ** argv)
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    init_stopwords();
    g_docinfo_mgr = new DocInfoManager();
    g_docinfo_mgr->load(StringFormat("%s/part-*", FLAGS_docinfo_data.c_str()));
    g_index = new Index(StringFormat("%s/part-*", FLAGS_index_data.c_str()));

    HttpServerThread server;
    g_server_started = false;
    int retry_count = 0;
    server.Start();
    // Wait until server starts
    while (!g_server_started) {
        if (retry_count == 3) {
            break;
        }
        ++retry_count;
        ThisThread::Sleep(1000);
    }

    if (!g_server_started) {
        server.StopAndWaitForExit();
        return -1;
    }
    return EXIT_SUCCESS;
}
