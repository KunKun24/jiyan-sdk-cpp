#include "jiyan/SDK_Main.cpp"

static bool 是否创建成功 = false;
static std::string 创建提示;

void 建普通卡(const std::string& 卡密列表, int 天数)
{
    JiYanSDK_API::创建卡密::普通类型(卡密列表, 天数, 是否创建成功, 创建提示, [] {
        // 回调在请求线程执行：这里只记录，回主线程再更新界面
    });
}

void 建多设备卡(const std::string& 卡密列表, int 天数, int 最大设备数)
{
    JiYanSDK_API::创建卡密::多设备类型(卡密列表, 天数, 最大设备数, 是否创建成功, 创建提示);
}

// 等价写法（英文接口仍然可用）
void 演示兼容返回()
{
    JiYanSDK_API::Kami::Create::Ordinary("示例卡密", 1, 是否创建成功, 创建提示);
}

// 单次卡：服务端暂未开放，会直接返回失败与提示，不会误报成功。
void 建单次卡(const std::string& 卡密列表)
{
    JiYanSDK_API::创建卡密::单次类型(卡密列表, 1, 是否创建成功, 创建提示);
}
