#include "FileUtils.h"

#include <unistd.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>

#include <string>

#define LOG_TAG "GSPSFileUtils"

#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AAssetManager* s_AAssetManager = NULL;

namespace SkyDragon {

    void FileUtils::setAAssetManager(void* assetManager)
    {
        if (assetManager)
        {
            s_AAssetManager = (AAssetManager*)assetManager;
        }
    }

    Data FileUtils::getData(const char* filePath, bool forString = false)
    {
        if (filePath == NULL || filePath[0] == 0)
            return Data::Null;

        unsigned char* data = nullptr;
        long size = 0;

        do {
            if (filePath[0] != '/')
            {
                if (nullptr == s_AAssetManager)
                {
                    LOGE("s_AAssetManager is nullptr");
                    break;
                }
        
                // read asset data
                auto asset = AAssetManager_open(s_AAssetManager, filePath, AASSET_MODE_UNKNOWN);
                if (nullptr == asset) {
                    break;
                }
        
                off_t fileSize = AAsset_getLength(asset);
                if (forString)
                {
                    data = (unsigned char*) malloc(fileSize + 1);
                    data[fileSize] = '\0';
                }
                else
                {
                    data = (unsigned char*) malloc(fileSize);
                }
        
                int bytesread = AAsset_read(asset, (void*)data, fileSize);
                size = bytesread;
        
                AAsset_close(asset);
            }
            else
            {
                const char* mode = nullptr;
                if (forString)
                    mode = "rt";
                else
                    mode = "rb";
        
                FILE *fp = fopen(filePath, mode);
                if(!fp) break;
        
                fseek(fp,0,SEEK_END);
                auto fileSize = ftell(fp);
                fseek(fp,0,SEEK_SET);
    
                if (forString)
                {
                    data = (unsigned char*) malloc(fileSize + 1);
                    data[fileSize] = '\0';
                }
                else
                {
                    data = (unsigned char*) malloc(fileSize);
                }
                fileSize = fread(data,sizeof(unsigned char), fileSize,fp);
                fclose(fp);
        
                size = fileSize;
            }
        } while(false);

        Data ret;
        if (data == nullptr || size == 0)
            LOGD("Get data from %s failed!", filePath);
        else
            ret.fastSet(data, size);

        return ret;
    }

    bool FileUtils::isFileExistOnStorage(const char* filePath)
    {
        auto ret = access(filePath, 0) == 0;
        if (!ret) {
            LOGD("isFileExistOnStorage: %s not exist!", filePath);
        }
        
        return ret;
    }
    
    bool FileUtils::isFileExistInAssets(const char* filePath)
    {
        if (filePath) {
            auto asset = AAssetManager_open(s_AAssetManager, filePath, AASSET_MODE_UNKNOWN);
            if (asset) {
                AAsset_close(asset);
                return true;
            }
        }
        
        return false;
    }
}
