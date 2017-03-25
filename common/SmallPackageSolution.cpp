#include "SmallPackageSolution.h"

#include <time.h>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <unordered_map>

#include <android/log.h>
#include <android/asset_manager_jni.h>

#include "gsps_init_generated.h"
#include "gsps_game_config_generated.h"
#include "gsps_game_res_config_generated.h"

#include "FileUtils.h"
#include "JniHelper.h"

#define MODE_STR_SUB_PACKAGE "sub_package"
#define MODE_STR_VERIFY_PACKAGE "verify_package"
#define MODE_STR_NORMAL "normal"
#define MODE_STR_EXPERIENCE "experience"

#define DOWNLOAD_MODE_SILENT 1
#define DOWNLOAD_MODE_FOREGROUND 2

#define FILE_GSPS_INIT "gsps_init.bin"
#define FILE_GSPS_GAME_CONFIG "gsps_game_config.bin"
#define FILE_GSPS_RES_CONFIG "gsps_game_res_config.bin"

#define LOG_TAG "GSPS"

#define SUB_PACKAGE_LOG(...)  __android_log_print(ANDROID_LOG_INFO, MODE_STR_SUB_PACKAGE, __VA_ARGS__)
#define VERIFY_PACKAGE_LOG(...)  __android_log_print(ANDROID_LOG_INFO, MODE_STR_VERIFY_PACKAGE, __VA_ARGS__)

#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

int s_GSPSRunMode = MODE_NORMAL;

namespace SkyDragon {

static std::string s_resSavePath;
static std::string s_sdCardRootPath;

static const GameConfig* s_gameConfig = nullptr;
static const GameResConfig* s_gameResConfig = nullptr;
//记录每一个资源对应的资源包索引
static std::unordered_map<std::string, int> s_resBundleIndexMap;
//静默下载的资源包索引队列
static std::queue<int> s_silentDownloadResBundleQueue;
static int s_resBundleCount = 0;

static std::thread* s_silentDownloadThread = nullptr;
static bool s_printAccessResourceEnable = false;
    
//用于控制移动网络环境下静默下载的流量消耗
static constexpr long DOWNLOAD_BYTE_LIMIT = 20 * 1024 * 1024;
//移动网络下累计下载的资源包大小
static long s_downloadByteWithMobile = 0;

extern "C"
{
    static void extensionSyncAPIJNI(const char*, const char*, int, double);
    extern int hookResAccessFuncs();
}
    
static void parseInitConfig()
{
    if (s_gameResConfig) {
        return;
    }
    
    auto initData = FileUtils::getData(FILE_GSPS_INIT, false);
    if (initData.isNull())
    {
        LOGW("Get data of init config failed!");
        return;
    }
    
    do {
        auto initConfig = GetGSPSInitConfig(initData.getBytes());
        if (initConfig == nullptr)
        {
            LOGE("Parse init config failed!");
            break;
        }

        auto runMode = initConfig->mode();
        if (runMode)
        {
            if (strcmp(MODE_STR_SUB_PACKAGE, runMode->c_str()) == 0)
                s_GSPSRunMode = MODE_SUB_PACKAGE;
            else if(strcmp(MODE_STR_VERIFY_PACKAGE, runMode->c_str()) == 0)
                s_GSPSRunMode = MODE_VERIFY_PACKAGE;
            else if(strcmp(MODE_STR_EXPERIENCE, runMode->c_str()) == 0)
                s_GSPSRunMode = MODE_EXPERIENCE_GAME;
            
            LOGD("GSPS mode:%s - %d", runMode->c_str(), s_GSPSRunMode);
            extensionSyncAPIJNI("setRunMode", "", s_GSPSRunMode, 0);
        }
        
        auto download_url = initConfig->download_url();
        if (download_url)
        {
            LOGD("download URL:%s", download_url->c_str());
            extensionSyncAPIJNI("setDownloadURL", download_url->c_str(), 0, 0);
        }
        else
        {
            LOGW("Parse download_url failed!");
        }
    } while(false);
}

static void parseResConfig()
{
    if (s_GSPSRunMode == MODE_SUB_PACKAGE || s_gameResConfig) {
        return;
    }
    
    static Data resConfigData;
    resConfigData = FileUtils::getData(FILE_GSPS_RES_CONFIG, false);
    if (resConfigData.isNull())
    {
        LOGW("Get data of res config failed!");
        return;
    }

    do
    {
        s_gameResConfig = GetGameResConfig(resConfigData.getBytes());
        if (nullptr == s_gameResConfig)
        {
            LOGE("Parse res confing failed!");
            break;
        }

        auto resBundles = s_gameResConfig->res_bundles();
        if (nullptr == resBundles)
        {
            LOGE("Parse res bundles failed!");
            break;
        }
        s_resBundleCount = resBundles->size();
        
        for (int bundleIndex = 0; bundleIndex < s_resBundleCount; ++bundleIndex)
        {
            auto bundle = resBundles->Get(bundleIndex);
            auto files = bundle->files();
            auto fileCount = files->size();
            for (int j = 0; j < fileCount; ++j)
            {
                s_resBundleIndexMap[files->Get(j)->c_str()] = bundleIndex;
            }
            s_silentDownloadResBundleQueue.push(bundleIndex);
        }
    } while(false);
}

extern "C" {
    static void startSilentDownload();
    
    static const char* RUNTIME_JNI_CLASS = "com/skydragon/smallpackage/NativeWrapper";

    void Java_com_skydragon_smallpackage_NativeWrapper_nativeInit(
        JNIEnv* env, jobject thiz, 
        jobject assetManager, jobject context,
        jstring resSavePath, jstring downloadSavePath, jstring sdCardRootPath)
    {
        JavaVM* gs_jvm = NULL;
        env->GetJavaVM(&gs_jvm);
        SkyDragon::JniHelper::setJavaVM(gs_jvm);
        JniHelper::setClassLoaderFrom(context);

        FileUtils::setAAssetManager(AAssetManager_fromJava(env, assetManager));
        
        auto c_resSavePath = env->GetStringUTFChars(resSavePath, NULL);
        //auto c_sdCardRootPath = env->GetStringUTFChars(sdCardRootPath, NULL);
        
        s_resSavePath = c_resSavePath;
        //s_sdCardRootPath = c_sdCardRootPath;
        
        env->ReleaseStringUTFChars(resSavePath, c_resSavePath);
        //env->ReleaseStringUTFChars(sdCardRootPath, c_sdCardRootPath);

        LOGD("###NativeWrapper_nativeInit %s", s_resSavePath.c_str());
        
        s_printAccessResourceEnable = false;
        parseInitConfig();
        parseResConfig();
        s_printAccessResourceEnable = true;
    }
    
    enum {
        EXTENSION_DOWNLOAD_RESBUNDLE_FAIL = 1,
        EXTENSION_NETWORK_STATUS,
        EXTENSION_ACTIVITY_STATUS
    };
    
    enum {
        NETWORK_TYPE_NONE = -1,
        NETWORK_TYPE_MOBILE = 0,
        NETWORK_TYPE_WIFI = 1,
        NETWORK_TYPE_WIMAX = 6,
        NETWORK_TYPE_ETHERNET = 9
    };
    static int sCurrNetworkType = NETWORK_TYPE_NONE;
    
    enum {
        ACTIVITY_CREATED = 1,
        ACTIVITY_STARTED,
        ACTIVITY_RESUMED,
        ACTIVITY_PAUSED,
        ACTIVITY_STOPPED,
        ACTIVITY_SAVE_INSTANCE_STATE,
        ACTIVITY_DESTROYED
    };
    static int sCurrActivityStatus = ACTIVITY_CREATED;
    
    void Java_com_skydragon_smallpackage_NativeWrapper_nativeExtensionAPI(
                                                                  JNIEnv* env, jobject thiz,
                                                                  jint extensionCode, jint intArg,
                                                                  jdouble doubleArg, jstring strArg)
    {
        switch (extensionCode) {
            case EXTENSION_DOWNLOAD_RESBUNDLE_FAIL:
                LOGD("nativeExtensionAPI DOWNLOAD_RESBUNDLE_FAIL index:%d", intArg);
                if (intArg >= 0 && intArg < s_resBundleCount) {
                    s_silentDownloadResBundleQueue.push(intArg);
                }
                break;
            case EXTENSION_NETWORK_STATUS:
                LOGD("nativeExtensionAPI EXTENSION_NETWORK_STATUS status:%d", intArg);
                if (sCurrNetworkType != intArg) {
                    sCurrNetworkType = intArg;
                    startSilentDownload();
                }
                break;
            case EXTENSION_ACTIVITY_STATUS:
                LOGD("nativeExtensionAPI EXTENSION_ACTIVITY_STATUS status:%d", intArg);
                sCurrActivityStatus = intArg;
                break;
            default:
                break;
        }
    }
    
    jstring Java_com_skydragon_smallpackage_NativeWrapper_nativeRealPath(JNIEnv* env, jobject thiz,
                                                                          jstring filePath)
    {
        auto c_filePath = env->GetStringUTFChars(filePath, NULL);
        if (s_GSPSRunMode != MODE_SUB_PACKAGE && c_filePath && c_filePath[0] != '/' &&
            s_resBundleIndexMap.find(c_filePath) != s_resBundleIndexMap.end()) {
            jstring jRealPath = env->NewStringUTF((s_resSavePath + c_filePath).c_str());
            env->ReleaseStringUTFChars(filePath, c_filePath);
            return jRealPath;
        } else {
            env->ReleaseStringUTFChars(filePath, c_filePath);
            return filePath;
        }
    }
    
    JNIEXPORT jint JNI_OnLoad(JavaVM* jvm, void* reserved)
    {
        hookResAccessFuncs();
        return JNI_VERSION_1_6;
    }
    
    static void extensionSyncAPIJNI(const char* method, const char* strArg, int intArg, double doubleArg)
    {
        JniMethodInfo t;
        
        if (JniHelper::getStaticMethodInfo(t, RUNTIME_JNI_CLASS, "extensionSyncAPI",
                                           "(Ljava/lang/String;Ljava/lang/String;ID)Ljava/lang/String;")) {
            jstring jmethod = t.env->NewStringUTF(method);
            jstring jstrArg = t.env->NewStringUTF(strArg ? strArg : " ");
            t.env->CallStaticObjectMethod(t.classID, t.methodID, jmethod, jstrArg, intArg, doubleArg);
            t.env->DeleteLocalRef(jmethod);
            t.env->DeleteLocalRef(jstrArg);
            t.env->DeleteLocalRef(t.classID);
        }
    }
    
    static void downloadResBundleJNI(const char* bundlePath, const char* md5, int size, int tag, int mode)
    {
        JniMethodInfo t;
        if (JniHelper::getStaticMethodInfo(t, RUNTIME_JNI_CLASS, "downloadResBundle",
                                           "(Ljava/lang/String;Ljava/lang/String;III)V")) {
            jstring jbundlePath = t.env->NewStringUTF(bundlePath);
            jstring jmd5 = t.env->NewStringUTF(md5);
            t.env->CallStaticVoidMethod(t.classID, t.methodID, jbundlePath, jmd5, size, tag, mode);
            t.env->DeleteLocalRef(jbundlePath);
            t.env->DeleteLocalRef(jmd5);
            t.env->DeleteLocalRef(t.classID);
        }
    }
}
    
static void startSilentDownload()
{
    if(s_gameResConfig == nullptr|| s_resBundleCount <= 0
       || s_silentDownloadThread != nullptr
       || s_silentDownloadResBundleQueue.empty()
       || sCurrNetworkType == NETWORK_TYPE_NONE) {
        return;
    }
    
    s_silentDownloadThread = new std::thread([](){
        LOGD("startSilentDownload, resource bundle count:%d", s_resBundleCount);
        auto bundles = s_gameResConfig->res_bundles();
        int bundleIndex;
        while (!s_silentDownloadResBundleQueue.empty()) {
            bundleIndex = s_silentDownloadResBundleQueue.front();
            
            auto bundle = bundles->Get(bundleIndex);
            if (bundle) {
                auto files = bundle->files();
                if (files) {
                    auto fileCount = files->size();
                    LOGD("Check resource bundle:%d, fileCount:%d", bundleIndex, fileCount);
                    for (int fileIndex = 0; fileIndex < fileCount && sCurrNetworkType != NETWORK_TYPE_NONE; ++fileIndex) {
                        auto filePath = files->Get(fileIndex);
                        if (filePath && !FileUtils::isFileExistOnStorage((s_resSavePath + filePath->c_str()).c_str())) {
                            //移动网络下，静默下载达到上限时停止
                            if (sCurrNetworkType == NETWORK_TYPE_MOBILE) {
                                if (s_downloadByteWithMobile + bundle->size() > DOWNLOAD_BYTE_LIMIT) {
                                    s_silentDownloadThread = nullptr;
                                    LOGW("Interrupt silent download! %ld:%d", s_downloadByteWithMobile, bundle->size());
                                    return;
                                }
                                s_downloadByteWithMobile += bundle->size();
                            }
                            
                            downloadResBundleJNI(bundle->path()->c_str(), bundle->md5()->c_str(),
                                              bundle->size(), bundleIndex, DOWNLOAD_MODE_SILENT);
                            break;
                        }
                    }
                }
            }
            
            s_silentDownloadResBundleQueue.pop();
            
            if (sCurrNetworkType == NETWORK_TYPE_NONE
                || sCurrActivityStatus == ACTIVITY_DESTROYED) {
                LOGW("Interrupt silent download! NetworkType:%d", sCurrNetworkType);
                break;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        }
        
        s_silentDownloadThread = nullptr;
        if (s_silentDownloadResBundleQueue.empty()) {
            LOGD("All resource bundle have been downloaded!");
            extensionSyncAPIJNI("allResourceBundleDownloaded", NULL, 0, 0);
        }
    });
}

void SmallPackageSolution::printAccessResource(const char* filePath)
{
    //分包模式下只输出log
    if (s_printAccessResourceEnable) {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        
        SUB_PACKAGE_LOG("#GSPS#%.3lf#GSPS#%s", (tv.tv_sec + tv.tv_usec/1000000.0), filePath);
    }
}
    
int SmallPackageSolution::preProcessResource(const char* filePath, std::string& newFilePath)
{
    if (filePath == nullptr || filePath[0] == 0)
    {
        return 0;
    }
    
    switch(s_GSPSRunMode)
    {
        case MODE_VERIFY_PACKAGE:
            if (s_GSPSRunMode == MODE_VERIFY_PACKAGE) {
                printAccessResource(filePath);
            }
        case MODE_NORMAL:
        case MODE_EXPERIENCE_GAME:
        {
            //判断是否包含在某一个资源包里
            if (s_gameResConfig) {
                auto it = s_resBundleIndexMap.find(filePath);
                if (it != s_resBundleIndexMap.end())
                {
                    newFilePath = s_resSavePath + filePath;
                    
                    if (!FileUtils::isFileExistOnStorage(newFilePath.c_str()))
                    {
                        auto resBundleIndex = it->second;
                        auto resBundle = s_gameResConfig->res_bundles()->Get(resBundleIndex);
                        
                        downloadResBundleJNI(resBundle->path()->c_str(), resBundle->md5()->c_str(),
                                          resBundle->size(), resBundleIndex, DOWNLOAD_MODE_FOREGROUND);
                    }
                    
                    return 1;
                }
            }
            break;
        }
        case MODE_SUB_PACKAGE:
        {
            printAccessResource(filePath);
            break;
        }
        default:
            break;
    }
    
    return 0;
}
    
const std::string& SmallPackageSolution::getResSavePath()
{
    return s_resSavePath;
}
    
int SmallPackageSolution::getGameMode()
{
    return s_GSPSRunMode;
}
    
bool SmallPackageSolution::isStrippedResource(const std::string &filePath)
{
    if (!filePath.empty() && s_resBundleIndexMap.find(filePath) != s_resBundleIndexMap.end()) {
        return true;
    }
        
    return false;
}

} // SkyDragon::
