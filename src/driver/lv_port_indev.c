#include "lv_port_indev.h"
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

static lv_indev_t *indev_keypad;

// Linux 文件描述符
static int evdev_fd = -1;

// --- 状态机结构体 ---
typedef struct {
  int physical_key_code; // 物理键值
  uint32_t press_start;  // 按下时的时间戳
  bool is_pressed;       // 当前物理状态
  bool long_press_sent;  // 标记长按事件是否已经发送过
} key_state_t;

static key_state_t key_state_1 = {0}; // 对应 KEY 1
static key_state_t key_state_2 = {0}; // 对应 KEY 2

// 用于缓存发送给 LVGL 的逻辑键
static uint32_t last_lv_key = 0;
// static lv_indev_state_t last_lv_state = LV_INDEV_STATE_RELEASED;

// 获取毫秒级时间戳
static uint32_t current_timestamp(void) { return lv_tick_get(); }

/**
 * @brief 底层读取 Linux Input Event，非阻塞
 */
static void evdev_read_phys(void) {
  struct input_event ev;
  int len;

  // 循环读取所有积压的事件
  while (1) {
    len = read(evdev_fd, &ev, sizeof(struct input_event));
    if (len != sizeof(struct input_event)) {
      break; // 没有更多数据或出错
    }

    if (ev.type == EV_KEY) {
      key_state_t *target = NULL;

      // 映射物理按键到状态对象
      if (ev.code == MY_KEY_1_CODE)
        target = &key_state_1;
      else if (ev.code == MY_KEY_2_CODE)
        target = &key_state_2;

      if (target) {
        if (ev.value == 1) { // 按下
          target->is_pressed = true;
          target->press_start = current_timestamp();
          target->long_press_sent = false; // 重置长按标记
        } else if (ev.value == 0) {        // 抬起
          target->is_pressed = false;
        }
      }
    }
  }
}

/**
 * @brief LVGL 回调函数：处理逻辑转换
 */
// static void keypad_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
//   // 1. 读取最新的物理硬件状态
//   evdev_read_phys();

//   uint32_t now = current_timestamp();

//   // 默认输出状态
//   data->state = LV_INDEV_STATE_RELEASED;
//   // 如果没有按键动作，保持发送上一次的键值 (LVGL 要求)
//   data->key = last_lv_key;

//   // --- 逻辑处理 ---
//   // 优先级：由于是单线程轮询，我们一次只能处理一个按键的逻辑
//   // 这里简单处理：优先处理正在活动的按键

//   key_state_t *active = NULL;
//   uint32_t short_key_map = 0;
//   uint32_t long_key_map = 0;

//   // 检查 Key 1
//   if (key_state_1.is_pressed || (key_state_1.press_start > 0)) {
//     active = &key_state_1;
//     short_key_map = LV_KEY_NEXT; // 短按：切换焦点
//     long_key_map = LV_KEY_PREV;  // 长按：反向切换
//   }
//   // 检查 Key 2 (如果 Key 1 没动静)
//   else if (key_state_2.is_pressed || (key_state_2.press_start > 0)) {
//     active = &key_state_2;
//     short_key_map = LV_KEY_ENTER; // 短按：确认
//     long_key_map = LV_KEY_ESC;    // 长按：退出/返回
//   }

//   if (active) {
//     // 计算按下时长
//     uint32_t duration = 0;
//     if (active->is_pressed) {
//       duration = now - active->press_start;
//     } else {
//       // 如果刚抬起，press_start 还需要用来计算最后一次时长
//       // 逻辑稍微复杂，为了简化，我们在抬起瞬间处理短按
//     }

//     // --- 场景 A: 按键保持按下状态 ---
//     if (active->is_pressed) {
//       // 检查是否达到长按阈值
//       if (duration > LONG_PRESS_MS) {
//         // 触发长按逻辑
//         data->key = long_key_map;
//         data->state = LV_INDEV_STATE_PRESSED;
//         active->long_press_sent = true; // 标记已触发

//         // 更新缓存
//         last_lv_key = data->key;
//       } else {
//         // 还没达到长按，什么都不发，等待用户是松开还是继续按
//         // 或者发送 Short Key 的 PRESSED 状态？
//         // 策略：为了区分长短按，在达到阈值前，我们不发送任何按键给 LVGL
//         data->state = LV_INDEV_STATE_RELEASED;
//       }
//     }
//     // --- 场景 B: 按键刚抬起 (物理松开) ---
//     else {
//       // 只有当 press_start 非 0 时才表示这是一个有效的按键周期结束
//       if (active->press_start != 0) {
//         uint32_t total_time = now - active->press_start;

//         // 如果之前没有触发过长按逻辑，并且时间较短 -> 判定为短按
//         if (!active->long_press_sent && total_time < LONG_PRESS_MS) {
//           // 这是一个短按点击！
//           // 问题：LVGL 需要探测到 PRESSED 然后 RELEASED 才能触发点击。
//           // 但我们是在抬起后才决定这是短按。
//           // 技巧：我们在这里必须欺骗 LVGL。
//           // 由于这是 polling，这帧我们发 PRESSED，下一帧发 RELEASED
//           // 比较难控制。 简单方案：直接发送 RELEASED，但是 key 是短按键值。
//           // *更稳妥方案*：这里发一次 PRESSED，利用静态变量下一帧发 RELEASED。

//           // 这里简化实现：假设按键按下时我们不反馈，只有松开时我们“点击”一下
//           // 但 LVGL 的 KEYPAD 模式通常需要看到按下动作。
//           // 让我们改进策略：
//           // 所有的按键按下，立即映射为 Short Key PRESSED。
//           // 如果超时，自动改为 Long Key PRESSED。
//           // 这样会有副作用：长按时会先触发短按的 Focus 变化。
//           // 但对于 NEXT/PREV 来说，先 Next 再 Prev
//           // 只是跳过去又跳回来，视觉上可以接受。
//         }

//         // 重置状态
//         active->press_start = 0;
//         active->long_press_sent = false;
//       }
//       data->state = LV_INDEV_STATE_RELEASED;
//     }
//   }
// }

// --- 改进后的回调逻辑 (更流畅的交互) ---
// 上面的逻辑在处理“按下不发，松开才发”时，会导致 UI 响应迟钝。
// 下面采用“即时响应 + 修正”策略：
// 1. 按下 Key1 -> 立即发送 LV_KEY_NEXT PRESSED
// 2. 按住超过 800ms -> 发送 LV_KEY_NEXT RELEASED, 紧接着发送 LV_KEY_PREV
// PRESSED
static void keypad_read_cb_v2(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  evdev_read_phys();
  uint32_t now = current_timestamp();

  data->state = LV_INDEV_STATE_RELEASED;
  data->key = last_lv_key;

  // 处理 Key 1 (Next / Prev)
  if (key_state_1.is_pressed) {
    uint32_t duration = now - key_state_1.press_start;
    if (duration < LONG_PRESS_MS) {
      data->key   = LV_KEY_RIGHT;
      data->state = LV_INDEV_STATE_PRESSED;
    } else {
      // 超时变身
      data->key   = LV_KEY_LEFT;
      data->state = LV_INDEV_STATE_PRESSED;
    }
  }
  // 处理 Key 2 (Enter / Esc)
  else if (key_state_2.is_pressed) {
    uint32_t duration = now - key_state_2.press_start;
    if (duration < LONG_PRESS_MS) {
      data->key = LV_KEY_ENTER;
      data->state = LV_INDEV_STATE_PRESSED;
    } else {
      // 超时变身
      data->key = LV_KEY_ESC;
      data->state = LV_INDEV_STATE_PRESSED;
    }
  }

  last_lv_key = data->key;
}

void lv_port_indev_init(void) {
  // 1. 打开 Linux 输入设备
  evdev_fd = open(INPUT_DEV_PATH, O_RDONLY | O_NONBLOCK);
  if (evdev_fd == -1) {
    perror("unable to open input device");
    return;
  }
  printf("Input device opened: %s\n", INPUT_DEV_PATH);

  // 2. 注册 LVGL 输入驱动
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);

  indev_drv.type = LV_INDEV_TYPE_KEYPAD; // 键盘模式
  indev_drv.read_cb = keypad_read_cb_v2; // 使用 V2 策略，响应更及时

  indev_keypad = lv_indev_drv_register(&indev_drv);
}

lv_indev_t *lv_port_indev_get_main(void) { return indev_keypad; }