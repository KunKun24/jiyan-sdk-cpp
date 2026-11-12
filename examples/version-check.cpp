/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */
    
void 检查当前SO()
{
    const auto so_name = JiYanSDK_Function::Get_Current_So_Name();
    const auto check = JiYanSDK_API::CloudUpdate::CheckVersionSync(so_name);
    // 也可指定 "libJiYan.so"；必须是已加载且磁盘文件可读取的同名 SO。
    // 对已知落盘路径：先 Get_File_MD5(path)，再 CheckVersionByMD5(file_name, md5)。
    // 按版本号比较：CheckVersionByVersionSync("1.5")，不混用空 MD5 代表本地成功。
    if (!check.request_ok) {
        // 网络/解密/验签/本地读取失败，显示 check.message；不能显示“已是最新”。
        return;
    }
    if (!check.cloud.has_file) {
        // 后台没有已发布更新文件；这与本地已是最新分开处理。
        return;
    }
    if (check.cloud.need_update == 1) {
        // 显示 check.cloud.version/file_size/latest_md5/update_content。
        // 按这个检查结果取链接，核对文件名与摘要，拒绝两次请求之间发生的发布变化。
        const auto download = JiYanSDK_API::CloudUpdate::GetDownloadUrlById(
            check.cloud.file_id, check.cloud.file_name, check.cloud.latest_md5);
        if (!download.request_ok) {
            // 显示 download.message，重新检查版本。
            return;
        }
        // 将 download.download 独立快照交给本次下载流程，见下载回执示例。
    } else {
        // 本次请求成功、有文件且 need_update==0，才能显示“已是最新”。
    }
}

// 旧异步接口仍可用，bool 的历史含义是“需要更新”，不是“请求成功/是否最新”。
// bool和string引用必须活到callback之后；需要完整可靠状态时优先使用上面的独立结果。
static bool 需要更新 = false;
static std::string 版本提示;
void 旧接口检查一次()
{
    JiYanSDK_API::CloudUpdate::CheckVersion("libJiYan.so", 需要更新, 版本提示, []() {
        // Data.cloud.request_ok 是兼容的本次请求状态；失败时需要更新=false。
        // 全局 Data 随请求改变，业务线程交接和并发读写仍由宿主管理。
        // 绝不能仅凭“需要更新==false”显示已是最新。
    });
}
