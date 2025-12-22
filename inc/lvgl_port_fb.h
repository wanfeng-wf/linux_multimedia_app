#ifndef _LVGL_PORT_FB_H
#define _LVGL_PORT_FB_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

// ST7735S
// #define MY_DISP_HOR_RES 160 // 水平分辨率
// #define MY_DISP_VER_RES 128 // 垂直分辨率

// ILI9341
#define MY_DISP_HOR_RES 320 // 水平分辨率
#define MY_DISP_VER_RES 240 // 垂直分辨率

// 定义缓冲区大小 (例如 10 行的高度)，减小内存占用
// 如果内存充足，可以设为全屏大小以获得更高性能
#define FB_BUF_SIZE_IN_PIXELS (MY_DISP_HOR_RES * MY_DISP_VER_RES) // 全屏大小

// 初始化 Linux Framebuffer 和 LVGL 显示驱动
int lv_port_fb_init(void);

// 退出清理 (清屏、关闭文件、释放映射)
void lv_port_fb_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // _LVGL_PORT_FB_H