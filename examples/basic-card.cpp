/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */
static std::string 公告内容;
void 加载公告() { 公告内容 = 获取公告(); }

ImGui::Begin("Debug");
static bool 是否验证成功 = false;
static std::string 卡密;
static std::string 提示;

if(!是否验证成功){
ImGui::BulletText("JiYanKami %s", JiYan_Cloud_Version);
//输入框
static char buffer[64];
strncpy(buffer, 卡密.c_str(), sizeof(buffer));
ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
ImGui::InputText("##卡密输入框", buffer, sizeof(buffer));

if(ImGui::Button("粘贴卡密",ImVec2(ImGui::GetContentRegionAvail().x,0))){
卡密 = Get_Clipboard_Text();
}

if(ImGui::Button("登陆卡密",ImVec2(ImGui::GetContentRegionAvail().x,0))){

static bool 使用卡密是否成功;
static std::string 使用卡密提示;
JiYanSDK_API::Kami::Use(卡密, 使用卡密是否成功, 使用卡密提示, []() {

是否验证成功 = 使用卡密是否成功;
提示 = 使用卡密提示;

});

}

if(ImGui::Button("解绑卡密",ImVec2(ImGui::GetContentRegionAvail().x,0))){

static bool 解绑卡密是否成功;
static std::string 解绑卡密提示;
JiYanSDK_API::Kami::Unbind(卡密, 解绑卡密是否成功, 解绑卡密提示, []() {

提示 = 解绑卡密提示;

});

}

ImGui::BulletText("错误提示: %s", 提示.c_str());
}else{
//悬浮窗其他控件
ImGui::Text("到期时间: %s", JiYanSDK_Result::Data.card.expire_text.c_str());
ImGui::Text("剩余秒: %lld", JiYanSDK_Result::Data.card.seconds_left);
ImGui::Text("在线人数: %d", JiYanSDK_Result::Data.online.count);
ImGui::Text("JiYan公告 %s", 公告内容.c_str()); //1.4新增
ImGui::Text("心跳间隔: %d 分钟", JiYanSDK_Result::Data.session.heartbeat_interval);
}

ImGui::End();