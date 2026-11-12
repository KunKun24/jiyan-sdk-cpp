# 云控与云更新

云控和云更新是两个独立能力：云控从 `cloud-client.php` 拉取远程开关、公告、按钮指令；云更新通过 `Cloudupdate.php` 检查发布文件、取下载地址并回执。两者都使用同一账号与应用配置；云控要求当前卡密已验证且设备码非空，云更新在取下载地址时同样要求设备码非空。服务端内部实现不在开源范围内，本页只描述客户端行为，字段语义以源码为准。

## 云控：绑定

| 面板类型 | 中文接口 | 变量类型 | 说明 |
| --- | --- | --- | --- |
| 开关 | `绑定开关(键, 变量)` | `bool*` | 勾选即改 |
| 滑动数值 | `绑定整数` / `绑定小数` | `int*` / `float*` | 拖动即改 |
| 文本 | `绑定文本(键, 变量)` | `std::string*` | 下发文本 |
| 下拉框 | `绑定下拉(键, 索引, 选项)` | `int*` + 选项表 | 索引对应选项顺序 |
| 单次指令 | `绑定按钮(键, 回调)` | `std::function<void()>` | 每次下发执行一次 |

键名必须匹配 `^[A-Za-z][A-Za-z0-9_]{0,63}$`，最多 100 个绑定；下拉选项 1–100 项、每项 1–64 字节且不能重复。重复键或重复变量地址会被拒绝，失败原因写入 `最后错误()`。绑定本身不读指针、不发网络请求，变量必须长期存活。

```cpp
#include "jiyan/SDK_Main.cpp"

struct 配置 {
    bool 显示帧率 = false;
    float 菜单缩放 = 1.0f;
    int 显示数量 = 10;
    int 模式索引 = 0;
    std::string 提示文本 = "欢迎";
};

static 配置 g_配置;
static const std::vector<std::string> 模式列表 = {"稳定", "性能", "极限"};

void 绑定云控() {
    JiYanSDK_API::云控::绑定开关("显示帧率", &g_配置.显示帧率);
    JiYanSDK_API::云控::绑定小数("菜单缩放", &g_配置.菜单缩放);
    JiYanSDK_API::云控::绑定整数("显示数量", &g_配置.显示数量);
    JiYanSDK_API::云控::绑定文本("提示文本", &g_配置.提示文本);
    JiYanSDK_API::云控::绑定下拉("模式", &g_配置.模式索引, 模式列表);
    JiYanSDK_API::云控::绑定按钮("重置布局", [] { /* 业务动作 */ });
}
```

## 云控：更新循环

在渲染线程或主循环每帧调用一次 `JiYanSDK_API::云控::更新()`。卡密已验证、身份就绪且至少有一个绑定时，第一次调用会启动后台工作线程；未验证、身份未就绪或已 `Shutdown()` 时直接返回，不产生网络请求。工作线程负责拉取与解析，按钮回调在调用 `更新()` 的线程执行。

```cpp
void 每帧() {
    JiYanSDK_API::云控::更新();
}

void 用户点了立即同步() {
    JiYanSDK_API::云控::立即同步();  // 下一次拉取不再等待轮询间隔
}
```

远程值有三种来源：`default` 只写一次，释放后恢复本地初值；`global` 与 `user` 在首次覆盖时保存本地值，之后每次 `更新()` 强制写入，身份变化或释放时恢复。`expires_at` 为 0 表示长期有效（按钮除外），过期后回落到 `default_value`。轮询间隔由服务端 `poll_after` 决定，范围 1–30 秒，默认 2 秒；失败后约 5 秒重试并保留尚未过期的覆盖值。

## 云控：按钮与 ACK

服务端下发的每条按钮指令都会先登记 pending ACK，再执行回调。回调抛异常或返回失败只影响该条 ACK，不会中断循环。ACK 状态只有成功/失败两种，对应文案包括 `执行成功`、`回调返回失败`、`回调异常：...`、`指令已过期，未执行`、`客户端未绑定该按钮`。只有本次请求实际发出的 ACK 才会被服务端确认；身份在回调与 ACK 之间变化时，旧 ACK 会被丢弃。

## 云控：状态与退出

`轮询间隔秒()` 返回服务端建议间隔；`最后错误()` 为空表示最近一次同步正常；`是否被远程覆盖(键)` 判断当前是否有生效的 `global`/`user` 覆盖。退出前调用 `RemoteConfig::GetInstance().Shutdown()` 恢复覆盖值、停止并 join 工作线程；`Shutdown()` 没有中文包装，且调用后 `更新()` 不会重新启动网络。

```cpp
// 退出流程
JiYanSDK_API::云控::更新();  // 停止前最后一次应用
RemoteConfig::GetInstance().Shutdown();
```

## 云更新：检查版本

| 接口 | 形态 | 说明 |
| --- | --- | --- |
| `检查版本(so 名, ok, msg)` | 异步 | 中文兼容接口，按 SO 文件名与本地 MD5 检查 |
| `检查版本按版本号(版本, ok, msg)` | 异步 | 中文兼容接口，按版本号检查 |
| `CloudUpdate::CheckVersionSync(so 名)` | 同步 | 读取已加载 SO 的 MD5 后检查 |
| `CloudUpdate::CheckVersionByMD5(文件名, md5)` | 同步 | 显式传本地 MD5，必须 32 位 hex |
| `CloudUpdate::CheckVersionByVersionSync(版本)` | 同步 | 服务端比较版本号 |

结果在 `Cloud_Update_Result`：`request_ok`、`message`、`cloud`。`cloud` 包含 `need_update`、`has_file`、`file_id`、`file_name`、`file_size`、`latest_md5`、`version`、`download_url`、`ticket`、`source`、`update_content`。`source` 取 `local` 或 `remote`；`local` 要求 `ticket` 与 `md5` 同时存在，客户端按服务端返回的字段校验，不自行推断文件位置。

```cpp
Cloud_Download_Result 准备本次下载(const std::string& so_name) {
    const auto check = JiYanSDK_API::CloudUpdate::CheckVersionSync(so_name);
    if (!check.request_ok || !check.cloud.has_file || check.cloud.need_update != 1)
        return {false, check.message, check.cloud, {}};
    return JiYanSDK_API::CloudUpdate::GetDownloadUrlById(
        check.cloud.file_id, check.cloud.file_name, check.cloud.latest_md5);
}
```

本地 MD5 读取失败时不要传空摘要；`CheckVersionSync` 会直接报 `本地 SO 文件无法读取或未加载，不能进行 MD5 检查`。文件名与发布记录不一致时检查会失败，避免拿到别的文件。

## 云更新：下载与回执

SDK 不实现下载器。宿主用自己的网络库按 `download_url` 下载，写盘并关闭文件后，把 `Cloud_Download_Result::download` 按值保存的快照传给 `ConfirmDownload`。确认前 SDK 会重新核对设备、应用、身份与 generation，并重新计算落盘文件的 MD5；任一不符都不会发出回执。

```cpp
Cloud_Update_Result 文件落盘后回执(const Cloud_Download_Snapshot& 快照,
                                  const std::string& 落盘路径) {
    const auto result = JiYanSDK_API::CloudUpdate::ConfirmDownload(快照, 落盘路径);
    if (!result.request_ok) return result;  // 保留快照与文件，重试同一票据
    // result.cloud.confirmed == 1 表示回执成功
    // charged == 1 表示本次扣次；charged == 0 可能是重复回执或外链
    // remaining_times == -1 表示响应未提供剩余次数
    return result;
}
```

回执失败时不要立刻申请新票据：网络不确定时保留原快照与完整文件，重试同一票据；服务端对同一票据重复回执不会重复扣次。`GetFileList` 属于管理操作，客户端调用会返回 `新 HTTP 客户端包不开放管理操作，请在 HTTPS 主站后台操作`。

## 公告与库状态

`JiYanSDK_API::卡密验证::获取公告()` 同步请求 `key.php` 的 `get_announcement`，返回 `data.announcement`，缺失时返回响应 `message`。`云更新::当前库状态()` 读取 `/proc/self/maps`：从 `/JiYan_Cloud/` 云更新库目录加载返回 0，其他自定义路径返回 1，未检测到返回 2。

云控与云更新的请求都经过同一协议层，因此加密、签名、`rc` 校验与错误码规则一致；排查时先看 `最后错误()` 与 `message`，再对照[错误码](errors.md)。
