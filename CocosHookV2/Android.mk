LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := SmallPackageSolution
LOCAL_SRC_FILES := CocosHook.cpp

LOCAL_STATIC_LIBRARIES := libhook
LOCAL_WHOLE_STATIC_LIBRARIES += libcommon

include $(BUILD_SHARED_LIBRARY)

#==============================================================
$(call import-module, ../common)
$(call import-module, ../Android-Inline-Hook)
