#include "lvgl_port_fb.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>

// --- Framebuffer 私有变量 ---
static int fbfd = 0;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static char *fbp           = 0;
static long int screensize = 0;

/**
 * @brief 控制背光
 * @param state 1 为开, 0 为关
 */
static void fbdev_set_backlight(int state)
{
    const char *path_pw = "/sys/class/backlight/fb_ili9341/bl_power";

    // bl_power 节点 (0:亮, 4:灭)
    FILE *fp = fopen(path_pw, "w");
    if (fp)
    {
        fprintf(fp, "%d", state ? 0 : 4);
        fclose(fp);
    }
}

/**
 * @brief 初始化 Framebuffer 设备 (/dev/fb1)
 */
static int fbdev_init(void)
{
    // 打开 ST7735S 对应的 fb1 设备
    fbfd = open("/dev/fb1", O_RDWR);
    if (fbfd == -1)
    {
        perror("Error: cannot open framebuffer device");
        return -1;
    }

    // 获取固定参数 (显存物理地址、行长度等)
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        perror("Error reading fixed information");
        return -1;
    }

    // 获取可变参数 (分辨率、色深)
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1)
    {
        perror("Error reading variable information");
        return -1;
    }

    printf("FB Device initialized: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    // 计算显存总大小
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

    // 内存映射 (mmap)
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((intptr_t)fbp == -1)
    {
        perror("Error: failed to map framebuffer device to memory");
        return -1;
    }

    return 0;
}

/**
 * @brief LVGL 刷新回调：将缓冲区数据拷贝到 Linux Framebuffer
 */
static void my_fb_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    // 边界检查
    if (fbp == NULL ||
        area->x2 < 0 || area->y2 < 0 ||
        area->x1 > (int)vinfo.xres - 1 || area->y1 > (int)vinfo.yres - 1)
    {
        lv_disp_flush_ready(drv);
        return;
    }

    // 计算当前刷新区域的宽度
    int32_t act_w = lv_area_get_width(area);

    // ST7735S 是 16bit (2 bytes/pixel)
    long int location       = 0;
    long int byte_per_pixel = vinfo.bits_per_pixel / 8;

    // 逐行拷贝
    for (int y = area->y1; y <= area->y2; y++)
    {
        // 计算目标 Framebuffer 的内存偏移量
        // 公式：(x + x_offset) * bpp + (y + y_offset) * line_length
        location = (area->x1 + vinfo.xoffset) * byte_per_pixel +
                   (y + vinfo.yoffset) * finfo.line_length;

        // 内存拷贝：将 LVGL 缓冲区的一行数据复制到显存映射区
        memcpy(fbp + location, (uint32_t *)color_p, act_w * byte_per_pixel);

        // 移动源数据指针到下一行
        color_p += act_w;
    }

    // 通知 LVGL 刷新完成
    lv_disp_flush_ready(drv);
}

/**
 * @brief 初始化显示
 */
int lv_port_fb_init(void)
{
    // 1. 初始化底层 Framebuffer
    if (fbdev_init() != 0)
    {
        return -1;
    }

    // 2. 初始化显示缓冲区
    // 使用静态数组作为 LVGL 的绘图缓冲区
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[FB_BUF_SIZE_IN_PIXELS];
    static lv_color_t buf2[FB_BUF_SIZE_IN_PIXELS];

    // 初始化缓冲区
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, FB_BUF_SIZE_IN_PIXELS);

    // 3. 注册显示驱动
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    // 使用从 ioctl 读取到的真实硬件分辨率
    disp_drv.hor_res = vinfo.xres;
    disp_drv.ver_res = vinfo.yres;

    disp_drv.flush_cb = my_fb_flush; // 设置回调
    disp_drv.draw_buf = &draw_buf;   // 设置缓冲

    // 注册
    lv_disp_drv_register(&disp_drv);
    // 打开背光
    fbdev_set_backlight(1);

    return 0;
}

/**
 * @brief 清屏并释放资源
 */
void lv_port_fb_deinit(void)
{
    // 关闭背光
    fbdev_set_backlight(0);

    if (fbp && screensize > 0)
    {
        printf("\nClearing screen...\n");
        // 清屏：全黑
        memset(fbp, 0, screensize);

        // 解除映射
        munmap(fbp, screensize);
    }

    if (fbfd > 0)
    {
        close(fbfd);
    }

    printf("Framebuffer closed.\n");
}