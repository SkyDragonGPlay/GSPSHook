#include <android/log.h>
#include <android/asset_manager.h>
#include <dlfcn.h>
#include <stdlib.h>

#include <unordered_map>

#include "SmallPackageSolution.h"

#define  LOG_TAG "CocosHook"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace SkyDragon;

//Cocos游戏动态库文件名
static const char* DYLIB_GAME = "___GSPS_GDYLIB___";

//符号地址: JNI_OnLoad
#if 0
static const char* SADDR_JNI_ONLOAD = "1870436";

static const char* SADDR_PLAY_EFFECT = "4152484";
static const char* SADDR_PLAY_MUSIC = "4152312";

#else

static const char* SADDR_JNI_ONLOAD = "___GSPS_ADDR_JOL___";

//符号地址: SimpleAudioEngine::playEffect
static const char* SADDR_PLAY_EFFECT = "___GSPS_ADDR_PE___";
//符号地址: SimpleAudioEngine::playBackgroundMusic
static const char* SADDR_PLAY_MUSIC = "___GSPS_ADDR_PM___";
#endif

//0:资源不存在或者仍然保留在APK里面  1:other
static std::unordered_map<void*, int> s_resMark;
extern int s_GSPSRunMode;

extern "C"
{
    #include "inlineHook.h"
    
    static AAsset* (*old_AAssetManager_open)(AAssetManager*, const char*, int) = NULL;
    static off_t (*old_AAsset_getLength)(AAsset* ) = NULL;
    static int (*old_AAsset_read)(AAsset* , void* , size_t ) = NULL;
    static void (*old_AAsset_close)(AAsset* ) = NULL;
    static int (*old_AAsset_openFD)(AAsset* asset, off_t* outStart, off_t* outLength) = NULL;

    static AAsset* new_AAssetManager_open(AAssetManager* mgr, const char* filePath, int mode)
    {
        if (s_GSPSRunMode == MODE_SUB_PACKAGE)
        {
            auto ret = old_AAssetManager_open(mgr, filePath, mode);
            if (ret) {
                SmallPackageSolution::printAccessResource(filePath);
            }
            return ret;
        }
        else
        {
            std::string newFilePath;
            int resMark = SmallPackageSolution::preProcessResource(filePath, newFilePath);
            if (resMark != 0)
            {
                FILE *fp = fopen(newFilePath.c_str(), "rb");
                if (fp)
                {
                    s_resMark[fp] = resMark;
                    return (AAsset*)fp;
                }
                else
                    return NULL;
            }
            else
            {
                return old_AAssetManager_open(mgr, filePath, mode);
            }
        }
    }

    static off_t new_AAsset_getLength(AAsset* asset)
    {
        if (s_GSPSRunMode != MODE_SUB_PACKAGE && asset && s_resMark.find(asset) != s_resMark.end())
        {
            FILE *fp = (FILE*)asset;
            fseek(fp, 0, SEEK_END);
            auto fileSize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            return fileSize;
        }
        else
        {
            return old_AAsset_getLength(asset);
        }
    }

    static int new_AAsset_read(AAsset* asset, void* buf, size_t count)
    {
        if (s_GSPSRunMode != MODE_SUB_PACKAGE && asset && s_resMark.find(asset) != s_resMark.end())
        {
            FILE *fp = (FILE*)asset;
            auto readSize = fread(buf, sizeof(unsigned char), count, fp);
            return readSize;
        }
        else
        {
            return old_AAsset_read(asset, buf, count);
        }
    }

    static void new_AAsset_close(AAsset* asset)
    {
        if (s_GSPSRunMode != MODE_SUB_PACKAGE && asset && s_resMark.find(asset) != s_resMark.end())
        {
            s_resMark.erase(asset);
            FILE *fp = (FILE*)asset;
            fclose(fp);
        }
        else
        {
            old_AAsset_close(asset);
        }
    }
    
    static int new_AAsset_openFD(AAsset* asset, off_t* outStart, off_t* outLength)
    {
        if (s_GSPSRunMode != MODE_SUB_PACKAGE && asset && s_resMark.find(asset) != s_resMark.end()) {
            FILE *fp = (FILE*)asset;
            return fileno(fp);
        }
        else
            return old_AAsset_openFD(asset, outStart, outLength);
    }
    
    static int hookFunc(const char* tag, uint32_t ori_func, uint32_t new_func, uint32_t **old_func_addr)
    {
        if (registerInlineHook(ori_func, new_func, old_func_addr) != ELE7EN_OK) {
            LOGD("registerH %s failed!", tag);
            return -1;
        }
        if (inlineHook(ori_func) != ELE7EN_OK) {
            LOGD("inlineH %s failed!", tag);
            return -1;
        }
        return 0;
    }
    
    int hookResAccessFuncs()
    {
        //hook AAssetManager_open
        hookFunc("AMO", (uint32_t) AAssetManager_open,
                 (uint32_t) new_AAssetManager_open,
                 (uint32_t **) &old_AAssetManager_open);
        
        //hook AAsset_getLength
        hookFunc("AGL", (uint32_t) AAsset_getLength,
                 (uint32_t) new_AAsset_getLength,
                 (uint32_t **) &old_AAsset_getLength);

        //hook AAsset_read
        hookFunc("AR", (uint32_t) AAsset_read,
                 (uint32_t) new_AAsset_read,
                 (uint32_t **) &old_AAsset_read);
    
        //hook AAsset_close
        hookFunc("AC", (uint32_t) AAsset_close,
                 (uint32_t) new_AAsset_close,
                 (uint32_t **) &old_AAsset_close);
        
        //hook AAsset_openFileDescriptor
        hookFunc("AOFD", (uint32_t) AAsset_openFileDescriptor,
                 (uint32_t) new_AAsset_openFD,
                 (uint32_t **) &old_AAsset_openFD);
        
        return 0;
    }
}

/*int unHook()
{
    if (inlineUnHook((uint32_t) puts) != ELE7EN_OK) {
        return -1;
    }

    return 0;
}*/
