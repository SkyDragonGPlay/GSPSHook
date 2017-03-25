#ifndef __SKYDRAGON_FILE_UTILS_H__
#define __SKYDRAGON_FILE_UTILS_H__

#include "Data.h"

namespace SkyDragon {

class FileUtils
{
public:
    static void setAAssetManager(void* assetManager);

    static Data getData(const char* filePath, bool forString);

    static bool isFileExistOnStorage(const char* filePath);
    
    static bool isFileExistInAssets(const char* filePath);
};

} //SkyDragon

#endif // __SKYDRAGON_FILE_UTILS_H__
