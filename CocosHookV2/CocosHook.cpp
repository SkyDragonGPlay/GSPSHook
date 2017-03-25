#include <android/log.h>
#include <jni.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <unordered_map>

#include "SmallPackageSolution.h"

#define  LOG_TAG "CocosHookV2"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace SkyDragon;

//Cocos游戏动态库文件名
static const char* DYLIB_GAME = "___GSPS_GDYLIB___";

//符号地址: JNI_OnLoad
#if 0
static const char* SADDR_JNI_ONLOAD = "1870436";
static const char* SSADDR_FILE_EXIST = "4097926";
static const char* SADDR_GET_FILE_DATA = "4097916";

static const char* SADDR_PLAY_EFFECT = "4152484";
static const char* SADDR_PLAY_MUSIC = "4152312";

#else

static const char* SADDR_JNI_ONLOAD = "___GSPS_ADDR_JOL___";

//符号地址: ZipFile::fileExists
static const char* SSADDR_FILE_EXIST = "___GSPS_ADDR_FE___";
//符号地址: FileUtils::getFileData
static const char* SADDR_GET_FILE_DATA = "___GSPS_ADDR_GFD___";

//符号地址: SimpleAudioEngine::playEffect
static const char* SADDR_PLAY_EFFECT = "___GSPS_ADDR_PE___";
//符号地址: SimpleAudioEngine::playBackgroundMusic
static const char* SADDR_PLAY_MUSIC = "___GSPS_ADDR_PM___";
#endif

static std::unordered_map<std::string, std::string> s_resFullPathMap;
extern int s_GSPSRunMode;

extern "C"
{
    #include "inlineHook.h"
    
    //原ZipFile::fileExists函数指针
    static bool (*old_fileExists)(void*, const std::string&) = NULL;
    //原ZipFile::getFileData函数指针
    static unsigned char* (*old_getFileData)(void*, const std::string&, unsigned long*) = NULL;
    
    //原dlopen函数指针
    static void* (*old_dlopen)(const char*, int) = NULL;
    
    static bool new_fileExists(void* that, const std::string& filePath)
    {
        //filePath带有前缀assets/
        if (s_GSPSRunMode == MODE_SUB_PACKAGE) {
            if (old_fileExists(that, filePath)) {
                SmallPackageSolution::printAccessResource(filePath.c_str() + 7);
                return true;
            }
            return false;
        }
        
        std::string newFilePath;
        int resMark = SmallPackageSolution::preProcessResource(filePath.c_str() + 7, newFilePath);
        if (resMark != 0) {
            s_resFullPathMap.emplace(filePath.c_str() + 7, newFilePath);
            return true;
        }
        else
            return old_fileExists(that, filePath);
    }
    
    static unsigned char* new_getFileData(void* that, const std::string& filePath, unsigned long* fileSize)
    {
        if (s_GSPSRunMode == MODE_SUB_PACKAGE)
        {
            return old_getFileData(that, filePath, fileSize);
        }
        else if(filePath.empty())
        {
            if (fileSize)
                *fileSize = 0;
            
            return NULL;
        }
        else
        {
            const char* relativePath;
            if (strncmp(filePath.c_str(), "assets/", 7) == 0)
                relativePath = filePath.c_str() + 7;
            else
                relativePath = filePath.c_str();
            
            auto fit = s_resFullPathMap.find(relativePath);
            if (fit != s_resFullPathMap.end())
            {
                unsigned char* data = NULL;
                long size = 0;
                
                FILE *fp = fopen(fit->second.c_str(), "rb");
                if (fp)
                {
                    fseek(fp, 0, SEEK_END);
                    size = ftell(fp);
                    if (size > 0)
                    {
                        fseek(fp, 0, SEEK_SET);
                        
                        data = (unsigned char*) malloc(size);
                        fread(data, sizeof(unsigned char), size, fp);
                    }
                    
                    fclose(fp);
                }
                
                if (fileSize)
                    *fileSize = size;
                
                return data;
            }
            else
            {
                return old_getFileData(that, filePath, fileSize);
            }
        }
    }
    
    static int hookFunc(const char* tag, uint32_t ori_func, uint32_t new_func, uint32_t **old_func_addr)
    {
        LOGD("hookFunc %s", tag);
        
        auto registerRet = registerInlineHook(ori_func, new_func, old_func_addr);
        if (registerRet != ELE7EN_OK) {
            LOGD("registerH %s failed! ret:%d", tag, registerRet);
            return -1;
        }
        
        auto inlineRet = inlineHook(ori_func);
        if (inlineRet != ELE7EN_OK) {
            LOGD("inlineH %s failed! ret:%d", tag, inlineRet);
            return -1;
        }
        return 0;
    }
    
    // referFuncMemoryAddr 用来参照的函数内存地址
    // referFuncFileAddr 用来参照的函数的so地址
    // destFuncFileAddr 目标函数的so地址
    static uint32_t getFuncMemoryAddr(uint32_t referFuncMemoryAddr, uint32_t referFuncFileAddr, uint32_t destFuncFileAddr)
    {
        uint32_t result = 0;
        if(referFuncFileAddr > destFuncFileAddr)
            result = referFuncMemoryAddr - (referFuncFileAddr - destFuncFileAddr);
        else
            result = referFuncMemoryAddr + (destFuncFileAddr - referFuncFileAddr);
        return result;
    }
    
    static void* new_dlopen(const char* filename, int flag)
    {
        void *libHandle = old_dlopen(filename, flag);
        if (libHandle && (strstr(filename, "libcocos") || strstr(filename, DYLIB_GAME)))
        {
            const char *error = dlerror();
            
            void *jniOnLoadHandle = dlsym(libHandle, "JNI_OnLoad");
            if((error = dlerror()) != NULL)
            {
                LOGE("JNI_OnLoad: %s", error);
            }
            else
            {
                uint32_t jniOnLoadMemoryAddr = (uint32_t)jniOnLoadHandle;
                auto jniOnLoadSAddr = atol(SADDR_JNI_ONLOAD);
                auto fileExistsSAddr = atol(SSADDR_FILE_EXIST);
                auto getFileDataSAddr = atol(SADDR_GET_FILE_DATA);
                
                uint32_t memoryAddr = getFuncMemoryAddr(jniOnLoadMemoryAddr, jniOnLoadSAddr, fileExistsSAddr);
                hookFunc("FE", memoryAddr, (uint32_t)new_fileExists, (uint32_t**)&old_fileExists);
                
                memoryAddr = getFuncMemoryAddr(jniOnLoadMemoryAddr, jniOnLoadSAddr, getFileDataSAddr);
                hookFunc("GFD", memoryAddr, (uint32_t)new_getFileData, (uint32_t**)&old_getFileData);
            }
        }
        
        return libHandle;
    }
    
    int hookResAccessFuncs()
    {
        return hookFunc("dlo", (uint32_t) dlopen, (uint32_t) new_dlopen, (uint32_t **) &old_dlopen);
    }
}
