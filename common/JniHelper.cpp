#include "JniHelper.h"
#include <pthread.h>
#include <string>
#include <android/log.h>

#define  LOG_TAG  "GSPSJniHelper"

#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace std;
namespace SkyDragon {

static JavaVM* sJavaVM = NULL;
jmethodID JniHelper::sLoadClassMethodID = nullptr;
jobject JniHelper::sClassLoader = nullptr;
    
    jclass JniHelper::getClassID(const char *className, JNIEnv *env) {
        if (nullptr == className) {
            return nullptr;
        }
        
        jstring jstrClassName = env->NewStringUTF(className);
        
        jclass _clazz = (jclass) env->CallObjectMethod(JniHelper::sClassLoader,
                                                       JniHelper::sLoadClassMethodID,
                                                       jstrClassName);
        
        if (nullptr == _clazz) {
            LOGE("Classloader failed to find class of %s", className);
            env->ExceptionClear();
        }
        
        env->DeleteLocalRef(jstrClassName);
        
        return _clazz;
    }

JavaVM* JniHelper::getJavaVM() {
    return sJavaVM;
}

    static pthread_key_t g_key;
    
    static void _detachCurrentThread(void* a) {
        pthread_t thisThread = pthread_self();
        LOGD("JniHelper::_detachCurrentThread(%p), pthread_self() = %ld", sJavaVM, thisThread);
        
        sJavaVM->DetachCurrentThread();
    }
    
void JniHelper::setJavaVM(JavaVM *javaVM) {
    sJavaVM = javaVM;
    
    pthread_key_create(&g_key, _detachCurrentThread);
}
    
    JNIEnv* JniHelper::cacheEnv(JavaVM* jvm) {
        JNIEnv* _env = nullptr;
        // get jni environment
        jint ret = jvm->GetEnv((void**)&_env, JNI_VERSION_1_6);
        
        switch (ret) {
            case JNI_OK :
                // Success!
                pthread_setspecific(g_key, _env);
                return _env;
                
            case JNI_EDETACHED :
                // Thread not attached
                if (jvm->AttachCurrentThread(&_env, nullptr) < 0)
                {
                    LOGE("Failed to get the environment using AttachCurrentThread()");
                    return nullptr;
                }
                else {
                    // Success : Attached and obtained JNIEnv!
                    pthread_setspecific(g_key, _env);
                    return _env;
                }
                break;
            case JNI_EVERSION :
                // Cannot recover from this error
                LOGE("JNI interface version 1.6 not supported");
            default :
                LOGE("Failed to get the environment using GetEnv()");
                return nullptr;
        }
    }

JNIEnv* JniHelper::getEnv()
{
    JNIEnv *_env = (JNIEnv *)pthread_getspecific(g_key);
    if (_env == nullptr)
    {
        _env = JniHelper::cacheEnv(sJavaVM);
    }
    
    return _env;
}

bool JniHelper::getStaticMethodInfo(JniMethodInfo &methodinfo, const char *className, const char *methodName, const char *paramCode) {
    if ((NULL == className) ||
        (NULL == methodName) ||
        (NULL == paramCode)) {
        return false;
    }

    JNIEnv *env = JniHelper::getEnv();
    if (!env) {
        LOGE("Failed to get JNIEnv! class:%s method:%s", className, methodName);
        return false;
    }

    jclass classID = JniHelper::getClassID(className, env);
    if (! classID) {
        LOGE("Failed to find class %s! methid:%s", className, methodName);
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return false;
    }

    jmethodID methodID = env->GetStaticMethodID(classID, methodName, paramCode);
    if (! methodID) {
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        LOGE("Failed to find static method id of %s.%s", className, methodName);
        return false;
    }

    methodinfo.classID = classID;
    methodinfo.env = env;
    methodinfo.methodID = methodID;
    return true;
}
    
    bool JniHelper::getMethodInfo_DefaultClassLoader(JniMethodInfo &methodinfo,
                                                     const char *className,
                                                     const char *methodName,
                                                     const char *paramCode) {
        if ((nullptr == className) ||
            (nullptr == methodName) ||
            (nullptr == paramCode)) {
            return false;
        }
        
        JNIEnv *env = JniHelper::getEnv();
        if (!env) {
            return false;
        }
        
        jclass classID = env->FindClass(className);
        if (! classID) {
            LOGE("Failed to find class %s", className);
            env->ExceptionClear();
            return false;
        }
        
        jmethodID methodID = env->GetMethodID(classID, methodName, paramCode);
        if (! methodID) {
            LOGE("Failed to find method id of %s", methodName);
            env->ExceptionClear();
            return false;
        }
        
        methodinfo.classID = classID;
        methodinfo.env = env;
        methodinfo.methodID = methodID;
        
        return true;
    }
    
    bool JniHelper::setClassLoaderFrom(jobject context) {
        JniMethodInfo _getclassloaderMethod;
        if (!JniHelper::getMethodInfo_DefaultClassLoader(_getclassloaderMethod,
                                                         "android/content/Context",
                                                         "getClassLoader",
                                                         "()Ljava/lang/ClassLoader;")) {
            return false;
        }
        
        JNIEnv *env = JniHelper::getEnv();
        jobject _c = env->CallObjectMethod(context,
                                            _getclassloaderMethod.methodID);
        
        if (nullptr == _c) {
            return false;
        }
        
        JniMethodInfo _m;
        if (!JniHelper::getMethodInfo_DefaultClassLoader(_m,
                                                         "java/lang/ClassLoader",
                                                         "loadClass",
                                                         "(Ljava/lang/String;)Ljava/lang/Class;")) {
            return false;
        }
        
        JniHelper::sClassLoader = env->NewGlobalRef(_c);
        JniHelper::sLoadClassMethodID = _m.methodID;
        
        return true;
    }

} // gplay::framework::
