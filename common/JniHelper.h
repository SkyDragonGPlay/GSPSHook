#ifndef __SKYDRAGON_JNI_HELPER_H__
#define __SKYDRAGON_JNI_HELPER_H__

#include <jni.h>

namespace SkyDragon {

typedef struct JniMethodInfo_
{
    JNIEnv *    env;
    jclass      classID;
    jmethodID   methodID;
} JniMethodInfo;

class JniHelper
{
public:
    static JavaVM* getJavaVM();
    static void setJavaVM(JavaVM *javaVM);
    static JNIEnv* getEnv();

    static bool setClassLoaderFrom(jobject context);
    
    static bool getStaticMethodInfo(JniMethodInfo &methodinfo, const char *className,
                                    const char *methodName, const char *paramCode);

private:
    static jmethodID sLoadClassMethodID;
    static jobject sClassLoader;
    
    static JNIEnv* cacheEnv(JavaVM* jvm);
    
    static jclass getClassID(const char *className, JNIEnv *env);
    
    static bool getMethodInfo_DefaultClassLoader(JniMethodInfo &methodinfo,
                                                 const char *className,
                                                 const char *methodName,
                                                 const char *paramCode);
};

} //SkyDragon

#endif // __SKYDRAGON_JNI_HELPER_H__
