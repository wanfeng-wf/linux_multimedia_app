#include "app_image.h"
#include "lv_group.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

// --- 配置 ---
#define IMG_DIR_PATH "/root/multimedia_app" // 真实的 Linux 路径用于扫描
#define LV_FS_PREFIX "S:"                   // LVGL 映射的盘符前缀

#define MAX_FILES     50  // 最大支持图片数
#define MAX_FNAME_LEN 256 // 文件名最大长度

// --- 静态变量 ---
static char file_list[MAX_FILES][MAX_FNAME_LEN];
static int file_count    = 0;
static int current_index = 0;

static lv_obj_t *main_cont  = NULL; // 主容器
static lv_obj_t *img_obj    = NULL; // 图片对象
static lv_obj_t *label_info = NULL; // 文件名显示

// --- 函数声明 ---
static void scan_image_dir(void);
static void load_current_image(void);
static void app_img_event_cb(lv_event_t *e);

/**
 * @brief 扫描目录下的图片文件 (.png)
 */
static void scan_image_dir(void)
{
    DIR *d;
    struct dirent *dir;
    file_count = 0;

    d = opendir(IMG_DIR_PATH);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            // 跳过 . 和 ..
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                continue;

            // 简单的后缀判断 (只作为演示，生产环境可用 strcasestr 忽略大小写)
            if (strstr(dir->d_name, ".png") || strstr(dir->d_name, ".PNG"))
            {
                if (file_count < MAX_FILES)
                {
                    snprintf(file_list[file_count], MAX_FNAME_LEN, "%s", dir->d_name);
                    file_count++;
                }
            }
        }
        closedir(d);
    }
    printf("App Image: Found %d images.\n", file_count);
}

/**
 * @brief 加载当前索引的图片
 */
static void load_current_image(void)
{
    if (file_count == 0)
    {
        lv_label_set_text(label_info, "No Images Found!");
        return;
    }

    // 限制索引范围
    if (current_index < 0)
        current_index = file_count - 1;
    if (current_index >= file_count)
        current_index = 0;

    // 拼接完整路径: S:filename.png
    // 注意：因为你在 lv_conf.h 里设置了 LV_FS_STDIO_PATH 为 "/root/multimedia_app/"
    // 所以这里只需要文件名即可
    char path_buf[128];
    snprintf(path_buf, sizeof(path_buf), "%s%s", LV_FS_PREFIX, file_list[current_index]);

    printf("Loading: %s\n", path_buf);

    // 设置图片源
    lv_img_set_src(img_obj, path_buf);

    // 更新底部文字
    lv_label_set_text_fmt(label_info, "[%d/%d] %s", current_index + 1, file_count, file_list[current_index]);
}

/**
 * @brief 退出应用回调
 */
static void close_app(void)
{
    if (main_cont)
    {
        lv_obj_del(main_cont);
        main_cont = NULL;
        img_obj   = NULL;
        // 这里可以添加逻辑返回主菜单
        printf("App Image Closed.\n");
    }
}

/**
 * @brief 按键事件处理
 */
static void app_img_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());

        printf("Key pressed: %d\n", key);
        // --- 核心交互逻辑 ---
        switch (key)
        {
            case LV_KEY_RIGHT: // 对应 Key 1 短按
            case LV_KEY_NEXT:  // 保留兼容
                current_index++;
                load_current_image();
                break;
            case LV_KEY_LEFT: // 对应 Key 1 短按
            case LV_KEY_PREV: // 保留兼容
                current_index--;
                load_current_image();
                break;
            case LV_KEY_ENTER: // 物理 Key 2 短按
                // 可选：切换全屏或旋转图片
                break;
            case LV_KEY_ESC: // 物理 Key 2 长按
                close_app();
                break;
        }
    }
}

/**
 * @brief 初始化图片应用
 */
void app_image_init(void)
{
    // 1. 扫描文件
    scan_image_dir();

    // 2. 创建主容器 (充当窗口)
    main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(main_cont, lv_color_black(), 0); // 黑色背景看着像相册
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);      // 禁止滚动条

    // 3. 必须添加到 Group 才能接收按键事件!
    lv_group_t *g = lv_group_get_default();
    if (g)
    {
        lv_group_add_obj(g, main_cont);
        lv_group_focus_obj(main_cont); // 强制聚焦，保证立即能响应按键
    }

    // 添加事件回调
    lv_obj_add_event_cb(main_cont, app_img_event_cb, LV_EVENT_KEY, NULL);

    // 4. 创建图片控件
    img_obj = lv_img_create(main_cont);
    lv_obj_align(img_obj, LV_ALIGN_CENTER, 0, -10);

    // 5. 创建底部信息栏
    label_info = lv_label_create(main_cont);
    lv_obj_align(label_info, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_color(label_info, lv_color_white(), 0);
    lv_label_set_text(label_info, "Loading...");

    // 6. 加载第一张图片
    current_index = 0;
    load_current_image();
}