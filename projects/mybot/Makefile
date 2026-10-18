ifeq ($(strip $(SDK_DIR)),)
    $(error SDK_DIR is required. Run scripts/prepare_vendor.py --language en-US or --language zh-CN from the repository root and use the printed build command)
endif

PROJECT_MAKE_FILE := $(SDK_DIR)/tools/build_tools/build_files/project_main.mk
ifeq ($(wildcard $(PROJECT_MAKE_FILE)),)
    $(error $(PROJECT_MAKE_FILE) does not exist. Run scripts/prepare_vendor.py and use its staged SDK_DIR)
endif
include $(PROJECT_MAKE_FILE)
