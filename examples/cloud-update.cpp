/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

Cloud_Download_Result 准备本次下载(const std::string& so_name)
{
    const auto check = JiYanSDK_API::CloudUpdate::CheckVersionSync(so_name);
    if (!check.request_ok || !check.cloud.has_file || check.cloud.need_update != 1)
        return {false, check.message, check.cloud, {}};
    return JiYanSDK_API::CloudUpdate::GetDownloadUrlById(
        check.cloud.file_id, check.cloud.file_name, check.cloud.latest_md5);
    // 如果本地SO尚未加载，可用 CheckVersionByMD5(name, JiYanSDK_Function::Get_File_MD5(path))，
    // 或 CheckVersionByVersionSync(version)。本地MD5失败不能当成空摘要发出。
    // 按名称兼容：GetDownloadUrlSync(name) 内部先查同名文件再核对取链接的名称/摘要。
}

// 在宿主已有下载器完成且关闭写入文件后调用；必须传真实落盘文件路径。
Cloud_Update_Result 文件落盘后回执(const Cloud_Download_Snapshot& 本次下载,
                                  const std::string& 落盘路径)
{
    // 本次下载必须按值保存 准备本次下载(...).download，不能从全局 Data 拼凑票据。
    // ConfirmDownload 会自己重新读取文件并核MD5；无法读取/不匹配时不发任何回执。
    // SDK在发请求前核对设备/应用/身份/generation，拒绝使用旧身份票据。
    const auto result = JiYanSDK_API::CloudUpdate::ConfirmDownload(本次下载, 落盘路径);
    if (!result.request_ok) {
        // 显示 result.message。网络不确定时保留原快照与完整落盘文件，重试同一票据；
        // 不盲目申请新票据。服务端同一票据重复回执不会重复扣次。
        return result;
    }
    // result.cloud.confirmed == 1：回执成功。
    // charged==1 表示本次扣次；charged==0 可能是重复回执或外链无需扣次。
    // remaining_times 是本次响应剩余次数，-1表示响应没提供。
    return result;
}

