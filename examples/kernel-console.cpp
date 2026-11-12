//终端请求记得初始化调用

JiYanSDK::Enable_SingleThreaded();

//启用单线程

//终端验证完毕，启用悬浮窗的时候调用

JiYanSDK::Close_SingleThreaded();

//关闭单线程，就行了，具体接口调用与ImGui窗口同样