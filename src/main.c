#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include "lvgl.h"
#include "lvgl_port_fb.h"

static volatile sig_atomic_t keep_running = 1;
void int_handler(int dummy) { keep_running = 0; }

int main(void)
{
    signal(SIGINT, int_handler);

    // LVGL 核心初始化
    lv_init();

    // 打开 /dev/fb1 并注册驱动
    if (lv_port_fb_init() != 0)
    {
        printf("Failed to init framebuffer!\n");
        return -1;
    }

    while (keep_running)
    {
        uint32_t time_until_next = lv_timer_handler();
        if (time_until_next > 5)
            time_until_next = 5;
        usleep(time_until_next * 1000);
    }

    lv_port_fb_deinit();

    return 0;
}