#ifndef __SKYDRAGON_GSPS_HELPER_H__
#define __SKYDRAGON_GSPS_HELPER_H__

#include <string>

namespace SkyDragon {

#define MODE_SUB_PACKAGE 1
#define MODE_VERIFY_PACKAGE 2
#define MODE_NORMAL 3
//试玩游戏
#define MODE_EXPERIENCE_GAME 4
    
class SmallPackageSolution
{
public:
    /**
     * 获取保存下载资源的根目录
     */
    static const std::string& getResSavePath();
    
    /**
     * 预处理访问的资源，输出log或者下载相关的资源包
     *
     * newFilePath:输出参数，原APK资源对应的外部资源路径；
     * 如果没有使用这个输出参数，之后可通过getResSavePath拼接新路径
     *
     * @return 0：不需要重定向资源路径  1：需要重定向资源路径
     */
    static int preProcessResource(const char* filePath, std::string& newFilePath);
    
    /**
     * 获取当前游戏运行模式
     */
    static int getGameMode();
    
    static void printAccessResource(const char* filePath);
    
    /**
     * 判断资源是否属于小包剥离的资源
     */
    static bool isStrippedResource(const std::string& filePath);
};

} //SkyDragon

#endif // __SKYDRAGON_GSPS_HELPER_H__
