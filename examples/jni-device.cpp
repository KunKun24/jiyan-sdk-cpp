/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

std::string 获取剪贴板内容() {
    if (g_App == nullptr || g_App->activity == nullptr) {
        return "错误: g_App或activity为空指针";
    }
    
    JavaVM* java_vm = g_App->activity->vm;
    JNIEnv* java_env = nullptr;
    
    jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
    bool should_detach = false;
    
    if (jni_return == JNI_EDETACHED) {
        jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
        if (jni_return != JNI_OK || java_env == nullptr) {
            return "错误: 无法附加到JNI线程";
        }
        should_detach = true;
    } else if (jni_return != JNI_OK || java_env == nullptr) {
        return "错误: 获取JNI环境失败";
    }
    
    std::string result = "";
    
    try {
        jclass context_class = java_env->FindClass("android/content/Context");
        if (context_class == nullptr) {
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到Context类";
        }
        
        jmethodID get_system_service_method = java_env->GetMethodID(
            context_class, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
        
        if (get_system_service_method == nullptr) {
            java_env->DeleteLocalRef(context_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到getSystemService方法";
        }
        
        jstring service_name = java_env->NewStringUTF("clipboard");
        if (service_name == nullptr) {
            java_env->DeleteLocalRef(context_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 无法创建服务名称字符串";
        }
        
        jobject clipboard_manager = java_env->CallObjectMethod(
            g_App->activity->clazz, get_system_service_method, service_name);
        
        java_env->DeleteLocalRef(service_name);
        java_env->DeleteLocalRef(context_class);
        
        if (clipboard_manager == nullptr) {
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 获取ClipboardManager实例失败";
        }
        
        jclass clipboard_class = java_env->FindClass("android/content/ClipboardManager");
        if (clipboard_class == nullptr) {
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到ClipboardManager类";
        }
        
        jmethodID get_primary_clip_method = java_env->GetMethodID(
            clipboard_class, "getPrimaryClip", "()Landroid/content/ClipData;");
        
        if (get_primary_clip_method == nullptr) {
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到getPrimaryClip方法";
        }
        
        jobject clip_data = java_env->CallObjectMethod(clipboard_manager, get_primary_clip_method);
        if (clip_data == nullptr) {
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "信息: 剪贴板为空";
        }
        
        jclass clip_data_class = java_env->FindClass("android/content/ClipData");
        if (clip_data_class == nullptr) {
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到ClipData类";
        }
        
        jmethodID get_item_at_method = java_env->GetMethodID(
            clip_data_class, "getItemAt", "(I)Landroid/content/ClipData$Item;");
        
        if (get_item_at_method == nullptr) {
            java_env->DeleteLocalRef(clip_data_class);
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到getItemAt方法";
        }
        
        jobject clip_item = java_env->CallObjectMethod(clip_data, get_item_at_method, 0);
        if (clip_item == nullptr) {
            java_env->DeleteLocalRef(clip_data_class);
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 获取剪贴板Item失败";
        }

        jclass clip_item_class = java_env->FindClass("android/content/ClipData$Item");
        if (clip_item_class == nullptr) {
            java_env->DeleteLocalRef(clip_data_class);
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到ClipData.Item类";
        }
        
        jmethodID get_text_method = java_env->GetMethodID(
            clip_item_class, "getText", "()Ljava/lang/CharSequence;");
        
        if (get_text_method == nullptr) {
            java_env->DeleteLocalRef(clip_item_class);
            java_env->DeleteLocalRef(clip_data_class);
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到getText方法";
        }
        
        jobject char_sequence = java_env->CallObjectMethod(clip_item, get_text_method);
        if (char_sequence == nullptr) {
            java_env->DeleteLocalRef(clip_item_class);
            java_env->DeleteLocalRef(clip_data_class);
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "信息: 剪贴板内容为空文本";
        }
        
        jclass char_sequence_class = java_env->FindClass("java/lang/CharSequence");
        if (char_sequence_class == nullptr) {
            java_env->DeleteLocalRef(clip_item_class);
            java_env->DeleteLocalRef(clip_data_class);
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到CharSequence类";
        }
        
        jmethodID to_string_method = java_env->GetMethodID(
            char_sequence_class, "toString", "()Ljava/lang/String;");
        
        if (to_string_method == nullptr) {
            java_env->DeleteLocalRef(char_sequence_class);
            java_env->DeleteLocalRef(clip_item_class);
            java_env->DeleteLocalRef(clip_data_class);
            java_env->DeleteLocalRef(clipboard_class);
            if (should_detach) java_vm->DetachCurrentThread();
            return "错误: 找不到toString方法";
        }
        
        jstring java_string = (jstring)java_env->CallObjectMethod(char_sequence, to_string_method);
        if (java_string != nullptr) {
            const char* c_str = java_env->GetStringUTFChars(java_string, nullptr);
            if (c_str != nullptr) {
                result = std::string(c_str);
                java_env->ReleaseStringUTFChars(java_string, c_str);
            } else {
                result = "错误: 无法获取字符串内容";
            }
            java_env->DeleteLocalRef(java_string);
        } else {
            result = "错误: 字符串转换失败";
        }
        
        java_env->DeleteLocalRef(char_sequence_class);
        java_env->DeleteLocalRef(clip_item_class);
        java_env->DeleteLocalRef(clip_data_class);
        java_env->DeleteLocalRef(clipboard_class);
        
    } catch (...) {
        if (should_detach) java_vm->DetachCurrentThread();
        return "错误: 发生未知异常";
    }
    
    if (should_detach) {
        java_vm->DetachCurrentThread();
    }
    
    return result.empty() ? "信息: 获取到空字符串" : result;
}





const char *GetAndroidID(JNIEnv *env, jobject context) {
    jclass contextClass = env->FindClass("android/content/Context");
    jmethodID getContentResolverMethod = env->GetMethodID(contextClass, "getContentResolver", "()Landroid/content/ContentResolver;");
    jclass settingSecureClass = env->FindClass("android/provider/Settings$Secure");
    jmethodID getStringMethod = env->GetStaticMethodID(settingSecureClass, "getString", "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");

    jobject obj = env->CallObjectMethod(context, getContentResolverMethod);
    jstring str = (jstring) env->CallStaticObjectMethod(settingSecureClass, getStringMethod, obj, env->NewStringUTF("android_id"));
    
    const char* result = env->GetStringUTFChars(str, 0);

    return result;
}

std::string 获取设备码() {
if (!g_App)
return ("");
auto activity = g_App->activity;
if (!activity)
return ("");

auto vm = activity->vm;
if (!vm)
return (""); 

auto object = activity->clazz;
if (!object)
return ("");

JNIEnv *env;
vm->AttachCurrentThread(&env, 0);

std::string result = GetAndroidID(env, object);
return result;
}


