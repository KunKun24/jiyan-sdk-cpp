/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

void DrawESP(){
auto Actors = GetActors();
ASTExtraPlayerCharacter *localPlayer = 0;
ASTExtraPlayerController *localController = 0;
for (int i = 0; i < Actors.size(); i++) {
auto Actor = Actors[i];
if (isObjectInvalid(Actor))
continue;
if (Actor->IsA(ASTExtraPlayerController::StaticClass())) {
localController = (ASTExtraPlayerController *) Actor;
break;
}
}

if (localController) {
for (int i = 0; i < Actors.size(); i++) {
auto Actor = Actors[i];
if (isObjectInvalid(Actor))
continue;
if (Actor->IsA(ASTExtraPlayerCharacter::StaticClass())) {
if (((ASTExtraPlayerCharacter *) Actor)->PlayerKey ==
localController->PlayerKey) {
localPlayer = (ASTExtraPlayerCharacter *) Actor;
break;
}
}
}

if (localPlayer) {
//确保自身指针"localPlayer有效"
static bool 同行识别是否刷新成功;
static std::string 同行识别刷新结果;

JiYanSDK_API::Gameinfo::Refresh(localPlayer->PlayerName.ToString(), localPlayer->PlayerUID.ToString(), 同行识别是否刷新成功, 同行识别刷新结果, []() {

if(!同行识别是否刷新成功){
//这里可以做错误处理
//例如
Log("[Gameinfo::Refresh is failure!!!] Because %s",同行识别刷新结果.c_str());
}

});

//这里"同行识别刷新结果"可以显示在屏幕上告诉用户刷新结果


if (Actor->IsA(ASTExtraPlayerCharacter::StaticClass())) {
auto Player = (ASTExtraPlayerCharacter *) Actor;
//绘制这里
bool 是否是同行 = JiYanSDK_API::Gameinfo::IsUser(Player->PlayerUID.ToString());
}
}}}


