#ifndef APP_MUSIC_H
#define APP_MUSIC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

// --- 播放状态枚举 ---
typedef enum
{
    MUSIC_STATE_STOPPED,
    MUSIC_STATE_PLAYING,
    MUSIC_STATE_PAUSED
} music_state_t;

// --- 控制接口 ---

// 初始化音乐播放器 (启动后台线程)
void app_music_init(void);

// 播放指定文件
// path: 文件的绝对路径 (例如 "/root/multimedia_app/song.mp3")
void music_play_file(const char *path);

// 暂停
void music_pause(void);

// 恢复播放
void music_resume(void);

// 停止 (回到开头)
void music_stop(void);

// 切换播放/暂停状态 (用于按键)
void music_toggle(void);

// --- 状态获取接口 (供 UI 定时器调用) ---

// 获取当前状态
music_state_t music_get_state(void);

// 获取总时长 (秒)
uint32_t music_get_total_time(void);

// 获取当前播放进度 (秒)
uint32_t music_get_current_time(void);

// 获取当前播放进度 (0-1000‰, 千分比，用于设置 Slider)
int music_get_progress_permille(void);

// 清理资源
void app_music_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // APP_MUSIC_H