/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SDK_RESULT_CPP
#define JIYAN_SDK_RESULT_CPP

#include <string>
#include <vector>
#include <cstdint>

struct Peer_Info {
    std::string user_name;
    std::string card_no;
    std::string device_id;
    std::string last_seen;
    long long   last_seen_ts = 0;
    int         is_self = 0;
};

struct Card_Info {
    std::string expire_text;
    long long   seconds_left = -1;
    std::string type;
    int         need_reverify = 0;
};

struct Online_Info {
    int                     count = 0;
    int                     is_peer = 0;
    int                     server_time = 0;
    std::vector<std::string> game_uids;
    std::vector<Peer_Info>   list;
};

struct Cloud_Info {
    bool        request_ok = false; // 本次完整请求成功；与 need_update 分开
    int         need_update = 0;
    int         has_file = 0;
    std::string file_id;
    std::string file_name;
    long long   file_size = 0;
    std::string latest_md5;
    std::string version;
    std::string download_url;
    std::string update_content;
    std::string ticket;
    std::string source;
    std::string file_md5;
    int         confirmed = 0;
    int         charged = 0;
    long long   remaining_times = -1; // -1 表示本次响应未提供
};

struct Cloud_Download_Snapshot {
    std::string file_id, file_name, file_md5, ticket, source, download_url;
    std::string device_id, app_id, developer_uid, api_url;
    std::string card_key, player_uid, player_name;
    std::uint64_t generation = 0;
    long long file_size = 0;
};

struct Cloud_Update_Result {
    bool request_ok = false;
    std::string message;
    Cloud_Info cloud;
};

struct Cloud_Download_Result {
    bool request_ok = false;
    std::string message;
    Cloud_Info cloud;
    Cloud_Download_Snapshot download;
};

struct Session_Info {
    int         heartbeat_interval = 30;   // 分钟（旧字段，保留兼容）
    int         heartbeat_seconds = 0;     // 秒级心跳（>0 优先）
    std::string rc;
};

struct Result_Data {
    bool        ok = false;
    int         code = 0;
    std::string message;
    std::string raw;

    Card_Info    card;
    Online_Info  online;
    Cloud_Info   cloud;
    Session_Info session;
};

namespace JiYanSDK_Result
{
    inline Result_Data Data;
    inline std::string current_response_result;
    inline int onlineCount = 0;
}

#endif