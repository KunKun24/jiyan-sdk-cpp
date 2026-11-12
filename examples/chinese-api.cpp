/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include "jiyan/SDK_Main.cpp"

void 初始化()
{
    JiYanSDK::set_device_id("a1b2c3d4e5f60718"); // 占位值，接入时替换成真实设备码

    Account_Info account;
    account.Account_Uid   = "JiYan_0000001";  // 开发者账号
    account.Account_Pass  = "安全请求密码";
    account.Account_Token = "安全请求Token";

    App_Info app;
    app.App_Pid          = "10001";           // 应用ID
    app.Crypto_Method    = 0;                 // 0-8，与后台「加密方式」一致
    app.Crypto_Key       = "后台配置的加解密密钥";
    app.Signature_Method = 0;                 // 0-14，与后台「签名方式」一致
    app.Signature_Key    = "";                // 不签名时留空

    JiYanSDK::init(account, app);
    JiYanSDK::set_api_url("http://api.xn--0try77c6um82w.cn/api/"); // 默认即此地址

    JiYanSDK::set_extra_params({{"channel", "official"}, {"version", "1.0.0"}});

    JiYanSDK::set_Global_Response_Handler([] {

        (void)JiYanSDK_Result::current_response_result;
    });
}

void 卡密流程(const std::string& 卡密)
{
    bool ok = false;
    std::string msg;

    JiYanSDK_API::卡密验证::使用卡密(卡密, ok, msg, [ok] {
        if (ok) {
            // 登录成功：在线人数、到期时间都在 JiYanSDK_Result::Data 里
            int 在线 = JiYanSDK_API::同行识别::在线人数();
            (void)在线;
        }
    });

    JiYanSDK_API::卡密验证::心跳(卡密, ok, msg);
    JiYanSDK_API::卡密验证::查询卡密(卡密, ok, msg);
}

void 同行流程(const std::string& 玩家名, const std::string& 玩家UID)
{
    bool ok = false;
    std::string msg;
    JiYanSDK_API::同行识别::刷新信息(玩家名, 玩家UID, ok, msg);
    if (JiYanSDK_API::同行识别::是否同行(玩家UID)) {
        // 命中同行
    }
    JiYanSDK_API::同行识别::立即刷新();
}

void 云更新流程(const std::string& so文件名)
{
    bool ok = false;
    std::string msg;
    JiYanSDK_API::云更新::检查版本(so文件名, ok, msg);
    if (ok && JiYanSDK_Result::Data.cloud.need_update == 1) {
        JiYanSDK_API::云更新::获取下载地址(so文件名, ok, msg);
    }
    int 状态 = JiYanSDK_API::云更新::当前库状态(); // 0 云更新库 1 其他路径 2 未检测到
    (void)状态;
}

void 建卡流程()
{
    bool ok = false;
    std::string msg;

    JiYanSDK_API::创建卡密::设置备注("活动赠送");
    JiYanSDK_API::创建卡密::普通类型("JY-AAA,JY-BBB", 30, ok, msg);        // 30 天，0=永久
    JiYanSDK_API::创建卡密::多设备类型("JY-CCC", 30, 3, ok, msg);           // 30 天，3 台设备
}
