LOCAL_PATH := $(call my-dir)

ROOT_PATH := $(LOCAL_PATH)/../..
SRC_PATH := $(ROOT_PATH)/src
LIB_PATH := $(ROOT_PATH)/vendor

include $(CLEAR_VARS)

LOCAL_MODULE := SFML_GRAPHICS

define walk
  $(wildcard $(1)) $(foreach e, $(wildcard $(1)/*), $(call walk, $(e)))
endef

FILES_ALL = $(call walk, $(SRC_PATH))
LOCAL_SRC_FILES := $(filter %.cpp, $(FILES_ALL))
LOCAL_SRC_FILES += $(filter %.c, $(FILES_ALL))

LOCAL_C_INCLUDES := $(ROOT_PATH)/include
LOCAL_C_INCLUDES += $(ROOT_PATH)/vendor/freetype/include
LOCAL_C_INCLUDES += $(ROOT_PATH)/vendor/stb/include

LOCAL_CPPFLAGS := -Os -DNDEBUG -Wno-deprecated -Wno-switch -Wno-unused-value
LOCAL_CPPFLAGS += -fdata-sections -ffunction-sections -Wl,--gc-sections

include $(BUILD_STATIC_LIBRARY)

$(call import-module,android/cpufeatures)