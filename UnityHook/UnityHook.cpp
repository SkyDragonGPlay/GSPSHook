#include <android/log.h>
#include <dlfcn.h>
#include <stdlib.h>

#include "SmallPackageSolution.h"

#define  LOG_TAG "UnityHook"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace SkyDragon;

extern "C"
{
    #include "inlineHook.h"

	static const char* JNI_ONLOAD_ADDR = "___GSPS_ADDR_JOA___";
	static const char* APK_OPEN_ADDR = "___GSPS_ADDR_AO___";
	static const char* IS_FILE_CREATED_ADDR = "___GSPS_ADDR_IFC___";
	static const char* WWW_CREATE_ADDR = "___GSPS_ADDR_WC___";

	static void *(*old_dlopen)(const char* libPath, int mode) = NULL;
	static int (*old_apkOpen)(const char* filePath) = NULL;
	static int (*old_isFileCreated)(std::string filePath) = NULL;
	static int (*old_WWWCreate)(char const *, char const *s, int, int, int, int, int, int, int) = NULL;

    static int hook(uint32_t ori_func, uint32_t new_func, uint32_t **old_func_addr)
	{
	    if (registerInlineHook(ori_func, new_func, old_func_addr) != ELE7EN_OK) {
	        return 0;
	    }
	    if (inlineHook(ori_func) != ELE7EN_OK) {
	        return 0;
	    }
	    return 1;
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
	
	static const char* getFileName(const char* path)
	{
	    char* tmp = (char*)path + strlen(path);
	    
	    do
	    {
	        if(*tmp == '/' || *tmp == '\\')
	            return tmp + 1;
	        tmp--;
	    }while(tmp != path);

	    return tmp;
	}

	static int getRelativeApkPath(const char* absolutePath, char* relativePath)
	{
	    int result = 0;

	    const char* tmp = strstr(absolutePath, ".apk");
	    if(tmp != NULL)
	    {
	        strcpy(relativePath, tmp + 5);
	        result = 1;
	    }
	    else if(strstr(absolutePath, "assets") == absolutePath)
	    {
	        strcpy(relativePath, absolutePath);
	        result = 1;
	    }
	    else if(strstr(absolutePath, "Managed") == absolutePath ||
	        strstr(absolutePath, "Resources") == absolutePath)
	    {
	        strcpy(relativePath, "assets/bin/Data/");
	        strcat(relativePath, absolutePath);
	        result = 1;
	    }
	    return result;
	}

	static int new_isFileCreated(std::string filePath)
	{
	    int result = 0;
	    char relativePath[256];
	    if(getRelativeApkPath(filePath.c_str(), relativePath) == 1)
	    {
	    	strcat(relativePath, ".obb");
	    	std::string tmp;
	    	int isRedirectResource = SmallPackageSolution::preProcessResource(relativePath, tmp);
	    	if(isRedirectResource == 1)
	    		result = 1;
	    	else
	    		result = old_isFileCreated(filePath);
	    }
	    else
	    {
	        result = old_isFileCreated(filePath);
	    }
	    return result;
	}

	static int new_apkOpen(const char* filePath)
	{
	    int result = 0;

	    char relativePath[512];
	    if(getRelativeApkPath(filePath, relativePath) == 1)
	    {
	    	strcat(relativePath, ".obb");
	    	std::string resSavePath;
	    	int isRedirectResource = SmallPackageSolution::preProcessResource(relativePath, resSavePath);

	    	if(isRedirectResource)
	    	{
	    		resSavePath += "/";
	    		resSavePath += getFileName(filePath);
	    		result = old_apkOpen(resSavePath.c_str());
	    	}
	    	else
	    		result = old_apkOpen(filePath);
	    }
	    else
	    {
	        result = old_apkOpen(filePath);
	    }
	    return result;
	}

	static int new_WWWCreate(char const *path, char const *a, int b, int c, int d, int e, int f, int g, int h)
	{
        if(path != NULL && strstr(path, "jar:file:///data/app/") != NULL)
        {
            char *relativePath = strstr(path + 21, ".apk!/");
            if(relativePath != NULL)
            {
            	std::string resSavePath;
		    	int isRedirectResource = SmallPackageSolution::preProcessResource(relativePath + 6, resSavePath);

		    	if(isRedirectResource)
		    	{
		    		resSavePath.insert(0, "file:///");
	    			return old_WWWCreate(resSavePath.c_str(), a, b, c, d, e, f, g, h);
		    	}
            }
        }

	    return old_WWWCreate(path, a, b, c, d, e, f, g, h);
	}

	static void *new_dlopen(const char* libPath, int mode)
	{
	    void *libHandle = NULL;

	    if(old_dlopen != NULL)
	    {
	        libHandle = old_dlopen(libPath, mode);

	        if(libHandle != NULL && strstr(libPath, "libunity.so"))
	        {
                uint32_t jniOnLoadAddr = (uint32_t)atoi(JNI_ONLOAD_ADDR);
	            uint32_t isFileCreatedAddr = (uint32_t)atoi(IS_FILE_CREATED_ADDR);
	            uint32_t apkOpenAddr = (uint32_t)atoi(APK_OPEN_ADDR);
	            uint32_t wwwCreateAddr = (uint32_t)atoi(WWW_CREATE_ADDR);
                if(jniOnLoadAddr == 0 || isFileCreatedAddr == 0 || apkOpenAddr == 0 || wwwCreateAddr == 0)
                {
                	LOGE("Not set functions' address into library file.");
                }
                
                {
	                const char *error = dlerror();

	                void *jniOnLoadHandle = dlsym(libHandle, "JNI_OnLoad");
	                if((error = dlerror()) != NULL)
                    {
	                    LOGE("dlsym JNI_OnLoad error: %s", error);
                        return libHandle;
                    }

	                uint32_t jniOnLoadMemoryAddr = (uint32_t)jniOnLoadHandle;

                    if (isFileCreatedAddr > 0) {
                        uint32_t isFileCreatedMemoryAddr = getFuncMemoryAddr(jniOnLoadMemoryAddr, jniOnLoadAddr, isFileCreatedAddr);
                        auto result = hook(isFileCreatedMemoryAddr, (uint32_t)new_isFileCreated, (uint32_t **) &old_isFileCreated);
                        if(result == 0)
                            LOGD("hook failed [%s] : %d", __FILE__, __LINE__);
                    }
					
                    if (apkOpenAddr > 0) {
                        uint32_t apkOpenMemoryAddr = getFuncMemoryAddr(jniOnLoadMemoryAddr, jniOnLoadAddr, apkOpenAddr);
                        auto result = hook(apkOpenMemoryAddr, (uint32_t)new_apkOpen, (uint32_t **) &old_apkOpen);
                        if(result == 0)
                            LOGD("hook failed [%s] : %d", __FILE__, __LINE__);
                    }
					
                    if (wwwCreateAddr > 0) {
                        uint32_t wwwCreateMemoryAddr = getFuncMemoryAddr(jniOnLoadMemoryAddr, jniOnLoadAddr, wwwCreateAddr);
                        auto result = hook(wwwCreateMemoryAddr, (uint32_t)new_WWWCreate, (uint32_t **) &old_WWWCreate);
                        if(result == 0)
                            LOGD("hook failed [%s] : %d", __FILE__, __LINE__);
                    }
                }
	        }
	    }

		return libHandle;
	}

    int hookResAccessFuncs()
    {
    	int result = hook((uint32_t)dlopen, (uint32_t)new_dlopen, (uint32_t **) &old_dlopen);

		if(result == 0)
			LOGD("hook failed [%s] : %d", __FILE__, __LINE__);
        return 0;
    }
}
