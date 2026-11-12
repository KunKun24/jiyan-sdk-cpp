/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SDK_CHINESE_CPP
#define JIYAN_SDK_CHINESE_CPP

#include <cstdio>
#include <fstream>
#include <sstream>

namespace JiYanSDK
{
    inline void set_extra_params(const nlohmann::json& params)
    {
        ExtraParams() = params.is_object() ? params : nlohmann::json::object();
    }

    inline void set_timestamp_check(bool enabled)
    {
        TimestampCheckEnabled() = enabled;
    }

    inline void set_Global_Response_Handler(std::function<void()> handler)
    {
        GlobalResponseHandler() = std::move(handler);
    }

    // 读取 /proc/self/maps，判断当前进程里云更新库的加载方式。
    inline int 当前库状态()
    {
        std::ifstream maps("/proc/self/maps");
        if (!maps.is_open()) return 2;
        bool found_cloud = false;
        bool found_other = false;
        const std::string cloud_mark = "/JiYan_Cloud/";
        std::string line;
        while (std::getline(maps, line))
        {
            if (line.find(".so") == std::string::npos) continue;
            if (line.find(cloud_mark) != std::string::npos && line.find("云更新库") != std::string::npos)
            {
                found_cloud = true;
                break;
            }
            if (line.find(cloud_mark) != std::string::npos) found_other = true;
        }
        if (found_cloud) return 0;
        if (found_other) return 1;
        return 2;
    }
}

namespace JiYanSDK_API
{
    namespace 卡密验证
    {
        inline void 使用卡密(std::string 卡密, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            Kami::Use(std::move(卡密), 成功, 说明, std::move(回调));
        }

        inline void 解绑设备(std::string 卡密, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            Kami::Unbind(std::move(卡密), 成功, 说明, std::move(回调));
        }

        inline void 心跳(std::string 卡密, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            Kami::Heartbeat(std::move(卡密), 成功, 说明, std::move(回调));
        }

        inline void 查询卡密(std::string 卡密, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            Kami::GetInfo(std::move(卡密), 成功, 说明, std::move(回调));
        }

        inline std::string 获取公告()
        {
            return ::获取公告();
        }
    }

    namespace 创建卡密
    {
        inline void 设置创建者(std::string 创建者) { Kami::Create::set_creator(std::move(创建者)); }
        inline void 设置备注(std::string 备注) { Kami::Create::set_notes(std::move(备注)); }

        // 普通卡：天数 <= 0 表示永久
        inline void 普通类型(std::string 卡密列表, int 天数, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            Kami::Create::Ordinary(std::move(卡密列表), 天数, 成功, 说明, std::move(回调));
        }

        // 多设备卡：天数 <= 0 表示永久，最大设备数 1..10000
        inline void 多设备类型(std::string 卡密列表, int 天数, int 最大设备数, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            Kami::Create::Multi_Device(std::move(卡密列表), 天数, 最大设备数, 成功, 说明, std::move(回调));
        }

        // 单次卡：服务端当前未开放该卡种
        inline void 单次类型(std::string 卡密列表, int 最大设备数, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            Kami::Create::Single(std::move(卡密列表), 最大设备数, 成功, 说明, std::move(回调));
        }
    }

    namespace 同行识别
    {
        inline void 刷新信息(std::string 玩家名, std::string 玩家UID, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            Gameinfo::Refresh(std::move(玩家名), std::move(玩家UID), 成功, 说明, std::move(回调));
        }

        inline void 立即刷新()
        {
            Gameinfo::Refresh_Now();
        }

        inline bool 是否同行(const std::string& 玩家UID)
        {
            return Gameinfo::IsUser(玩家UID);
        }

        inline int 在线人数()
        {
            return JiYanSDK_Result::Data.online.count;
        }
    }

    namespace 云更新
    {
        inline void 检查版本(std::string SO文件名, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            CloudUpdate::CheckVersion(std::move(SO文件名), 成功, 说明, std::move(回调));
        }

        inline void 检查版本按版本号(std::string 版本号, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            CloudUpdate::CheckVersionByVersion(std::move(版本号), 成功, 说明, std::move(回调));
        }

        inline void 获取下载地址(std::string 文件名, bool& 成功, std::string& 说明, std::function<void()> 回调 = nullptr)
        {
            CloudUpdate::GetDownloadUrl(std::move(文件名), 成功, 说明, std::move(回调));
        }

        // 0 = 云更新库目录加载；1 = 其他自定义路径；2 = 未检测到
        inline int 当前库状态()
        {
            return JiYanSDK::当前库状态();
        }
    }

    namespace 云控
    {
        inline bool 绑定开关(const std::string& 键, bool* 变量) { return RemoteConfig::GetInstance().BindValue(键, 变量); }
        inline bool 绑定整数(const std::string& 键, int* 变量) { return RemoteConfig::GetInstance().BindValue(键, 变量); }
        inline bool 绑定小数(const std::string& 键, float* 变量) { return RemoteConfig::GetInstance().BindValue(键, 变量); }
        inline bool 绑定文本(const std::string& 键, std::string* 变量) { return RemoteConfig::GetInstance().BindValue(键, 变量); }

        inline bool 绑定下拉(const std::string& 键, int* 索引, const std::vector<std::string>& 选项)
        {
            return RemoteConfig::GetInstance().BindSelect(键, 索引, 选项);
        }

        inline bool 绑定按钮(const std::string& 键, std::function<void()> 回调)
        {
            return RemoteConfig::GetInstance().BindButton(键, std::move(回调));
        }

        inline void 更新() { RemoteConfig::GetInstance().Update(); }
        inline void 立即同步() { RemoteConfig::GetInstance().RequestSync(); }
        inline int 轮询间隔秒() { return RemoteConfig::GetInstance().PollIntervalSeconds(); }
        inline std::string 最后错误() { return RemoteConfig::GetInstance().LastError(); }
        inline bool 是否被远程覆盖(const std::string& 键) { return RemoteConfig::GetInstance().IsRemoteOverride(键); }
    }
}

#endif
