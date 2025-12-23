#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// 使用 evtest 工具查看你的按键对应的 event 节点
#define INPUT_DEV_PATH "/dev/input/event3"

// 使用 evtest 查看你的驱动上报的键值 (Linux Kernel Keycode)
// 假设你的驱动定义的是 KEY_VOLUMEDOWN 和 KEY_VOLUMEUP，或者是 KEY_A / KEY_B
// 请根据实际情况修改这里！
#define MY_KEY_1_CODE 148 // 例如: KEY_VOLUMEUP (切换/上一曲)
#define MY_KEY_2_CODE 149 // 例如: KEY_VOLUMEDOWN (确认/下一曲)

// 长按判定阈值 (毫秒)
#define LONG_PRESS_MS 800

// --- 函数声明 ---

// 初始化输入设备
void lv_port_indev_init(void);

// 获取全局输入设备指针 (后续创建 Group 时需要用到)
lv_indev_t *lv_port_indev_get_main(void);

#ifdef __cplusplus
}
#endif

#endif /*LV_PORT_INDEV_H*/