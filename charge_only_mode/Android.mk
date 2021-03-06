ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),jordan)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	alarm.c \
	draw.c \
	events.c \
	hardware.c \
	screen.c \
	main.c

LOCAL_STATIC_LIBRARIES := libunz libcutils libc

LOCAL_SHARED_LIBRARIES := libhardware

LOCAL_C_INCLUDES := external/zlib
LOCAL_MODULE:= charge_only_mode.mb525
LOCAL_MODULE_STEM := charge_only_mode
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

endif 
