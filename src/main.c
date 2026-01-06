#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

// 引入所有 App 的头文件
#include "app_menu.h"
#include "app_image.h"
#include "app_text.h"
#include "app_music.h"
#include "app_video.h"

// --- 全局状态 ---
static volatile sig_atomic_t keep_running = 1;
static app_id_t g_current_app             = APP_ID_NONE; // 当前正在运行的 App

// --- 函数声明 ---
static void system_enter_menu(void);
static void on_menu_select(app_id_t id);
static void on_app_exit_request(void);
void int_handler(int dummy) { keep_running = 0; }

int main(void)
{
    signal(SIGINT, int_handler);

    // 1. 基础驱动初始化
    lv_init();
    if (lv_port_disp_init() < 0)
    {
        printf("Error: Display init failed\n");
        return -1;
    }
    lv_port_indev_init();

    // 2. 创建按键组 (Group) 基础设施
    // 全局默认组由各 App 内部自行获取 (lv_group_get_default) 并添加对象
    // 这里我们先创建一个空的默认组
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(lv_port_indev_get_main(), g);

    // 3. 进入系统主菜单
    system_enter_menu();

    // 4. 主循环
    while (keep_running)
    {
        uint32_t time_until_next = lv_timer_handler();

        // 限制最小休眠时间，避免 CPU 占用过高
        if (time_until_next > 10)
            time_until_next = 10;

        usleep(time_until_next * 1000);
    }

    // 5. 系统退出清理
    // 关闭当前正在运行的 App (如果有)
    if (g_current_app != APP_ID_NONE)
    {
        on_app_exit_request(); // 这会关闭 App 并开菜单
        app_menu_close();      // 再把菜单关了
    }
    else
    {
        app_menu_close();
    }

    lv_port_disp_deinit();
    printf("System Shutdown.\n");

    return 0;
}

/**
 * @brief 从应用返回菜单 (App Exit Callback)
 * 当任何 App 内部按下 ESC 时，会调用此函数
 */
static void on_app_exit_request(void)
{
    printf("System: Switching back to Menu...\n");

    // 1. 根据当前状态，关闭对应的 App
    switch (g_current_app)
    {
        case APP_ID_IMAGE:
            app_image_close();
            break;
        case APP_ID_TEXT:
            app_text_close();
            break;
        case APP_ID_MUSIC:
            app_music_close();
            break;
        case APP_ID_VIDEO:
            app_video_close();
            break;
        default:
            break;
    }

    g_current_app = APP_ID_NONE;

    // 2. 进入主菜单
    system_enter_menu();
}

/**
 * @brief 从菜单进入应用 (Menu Select Callback)
 * 当在菜单点击图标时，会调用此函数
 */
static void on_menu_select(app_id_t id)
{
    printf("System: Starting App ID %d...\n", id);

    // 1. 关闭菜单 UI
    app_menu_close();

    // 2. 启动对应的 App
    // 将 on_app_exit_request 作为回调传进去，这样 App 退出时能找回来
    switch (id)
    {
        case APP_ID_IMAGE:
            app_image_init(on_app_exit_request);
            g_current_app = APP_ID_IMAGE;
            break;

        case APP_ID_TEXT:
            app_text_init(on_app_exit_request);
            g_current_app = APP_ID_TEXT;
            break;

        case APP_ID_MUSIC:
            app_music_init(on_app_exit_request);
            g_current_app = APP_ID_MUSIC;
            break;

        case APP_ID_VIDEO:
            app_video_init(on_app_exit_request);
            g_current_app = APP_ID_VIDEO;
            break;

        default:
            // 如果 ID 不识别，重新回菜单
            printf("System: Unknown App ID, returning to menu.\n");
            system_enter_menu();
            break;
    }
}

/**
 * @brief 系统启动/复位进入菜单
 */
static void system_enter_menu(void)
{
    // 初始化菜单，并传入选择回调
    app_menu_init(on_menu_select);
}