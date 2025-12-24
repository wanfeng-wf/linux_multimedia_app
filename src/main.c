#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "app_image.h"
#include "app_text.h"

static volatile sig_atomic_t keep_running = 1;
void int_handler(int dummy) { keep_running = 0; }

int main(void)
{
    signal(SIGINT, int_handler);

    // LVGL 核心初始化
    lv_init();

    lv_port_disp_init();  // 初始化显示驱动
    lv_port_indev_init(); // 初始化输入按键

    // 创建一个全局 Group (用于按键导航)
    lv_group_t *g = lv_group_create();

    // 将输入设备关联到 Group
    lv_indev_set_group(lv_port_indev_get_main(), g);

    // 设置为默认组 (这很重要，之后创建的新控件会自动加入这个组)
    lv_group_set_default(g);

    // --- 测试 UI ---
    // 创建两个按钮测试焦点切换
    // lv_obj_t *btn1 = lv_btn_create(lv_scr_act());
    // lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);
    // lv_obj_t *label1 = lv_label_create(btn1);
    // lv_label_set_text(label1, "Button 1");

    // lv_obj_t *btn2 = lv_btn_create(lv_scr_act());
    // lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
    // lv_obj_t *label2 = lv_label_create(btn2);
    // lv_label_set_text(label2, "Button 2");

    // 手动把控件加入组 (如果没设默认组的话需要这一步)
    // lv_group_add_obj(g, btn1);
    // lv_group_add_obj(g, btn2);

    // app_image_init();
    app_text_init();

    while (keep_running)
    {
        uint32_t time_until_next = lv_timer_handler();
        if (time_until_next > 5)
            time_until_next = 5;
        usleep(time_until_next * 1000);
    }

    lv_port_disp_deinit();

    return 0;
}