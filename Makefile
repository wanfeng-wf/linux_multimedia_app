TARGET_EXEC = multimedia
BUILD_DIR = ./build

# --- 远程部署配置 ---
REMOTE_USER = root
REMOTE_IP   = 192.168.7.2
REMOTE_PATH = /root/$(TARGET_EXEC)

# --- 交叉编译工具链 ---
CROSS_COMPILE ?= aarch64-linux-gnu-
CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

# --- 目录配置 ---
# 源文件目录
SRC_DIRS := ./src \
			 ./3rdparty/lvgl/src

# 递归查找所有的 .c 文件
SRCS := $(shell find $(SRC_DIRS) -name '*.c')
# 自动查找包含路径
INC_DIRS := $(shell find $(SRC_DIRS) -type d)

# 追加目录
INC_DIRS += ./3rdparty/lvgl ./inc /usr/include/freetype2

INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# --- 编译参数 ---
# -Os: 针对体积优化
# -g:  保留调试信息 (strip 之前)
CFLAGS = -Os -g -Wall -ffunction-sections -fdata-sections $(INC_FLAGS) -DLV_CONF_INCLUDE_SIMPLE

# --- 链接参数 ---
LDFLAGS = -Wl,--gc-sections -flto
# 如果需要链接 math 库或 pthread，在这里添加 -lm -lpthread
LDLIBS = -lgpiod -lm -lpng -ljpeg -lz -lfreetype

# --- 构建逻辑 ---
TARGET = $(BUILD_DIR)/$(TARGET_EXEC)
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

all: $(TARGET)

# 链接
$(TARGET): $(OBJS)
	@echo "Linking $@"
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
	@echo "Build complete. File size:"
	@du -h $@

# 编译
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

# --- 部署与清理 ---
# 部署: 这里进行 strip，既保留了本地带符号的 build 版本，又上传了小体积版本
push: $(TARGET)
	@echo "Stripping and pushing to $(REMOTE_IP)..."
	@cp $(TARGET) $(TARGET).stripped
	@$(STRIP) $(TARGET).stripped
	@echo "Strip complete. File size:"
	@du -h $(TARGET).stripped
	@scp $(TARGET).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_PATH)
	@rm $(TARGET).stripped
	@echo "Done."

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

.PHONY: all clean push

-include $(DEPS)