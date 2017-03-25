LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := common
LOCAL_SRC_FILES := JniHelper.cpp \
                   Data.cpp \
                   FileUtils.cpp \
                   SmallPackageSolution.cpp \
                   ../AndroidExtract7z/7zAlloc.c \
                   ../AndroidExtract7z/7zArcIn.c \
                   ../AndroidExtract7z/7zBuf.c \
                   ../AndroidExtract7z/7zBuf2.c \
                   ../AndroidExtract7z/7zCrc.c \
                   ../AndroidExtract7z/7zCrcOpt.c \
                   ../AndroidExtract7z/7zDec.c \
                   ../AndroidExtract7z/CpuArch.c \
                   ../AndroidExtract7z/Delta.c \
                   ../AndroidExtract7z/LzmaDec.c \
                   ../AndroidExtract7z/Lzma2Dec.c \
                   ../AndroidExtract7z/Bra.c \
                   ../AndroidExtract7z/Bra86.c \
                   ../AndroidExtract7z/BraIA64.c \
                   ../AndroidExtract7z/Bcj2.c \
                   ../AndroidExtract7z/Ppmd7.c \
                   ../AndroidExtract7z/Ppmd7Dec.c \
                   ../AndroidExtract7z/7zFile.c \
                   ../AndroidExtract7z/7zStream.c \
                   ../AndroidExtract7z/Util/7z/7zMain.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
                    $(LOCAL_PATH)/../AndroidExtract7z
                    
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)

LOCAL_EXPORT_LDLIBS := -llog \
                       -landroid

include $(BUILD_STATIC_LIBRARY)
