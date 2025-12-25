#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "app_music.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

// --- 配置 ---
#define MUSIC_DIR_PATH "/root/multimedia_app"
#define MAX_FILES      50
#define MAX_FNAME_LEN  256

// --- 全局变量：音频引擎 ---
static ma_engine engine;
static ma_sound sound;
static bool is_engine_inited = false;
static bool is_sound_loaded  = false;
static float current_volume  = 0.8f; // 0.0 ~ 1.0

// --- 全局变量：文件列表 ---
static char file_list[MAX_FILES][MAX_FNAME_LEN];
static int file_count       = 0;
static int current_file_idx = 0;

// --- 全局变量：UI ---
static lv_obj_t *main_cont        = NULL;
static lv_obj_t *label_title      = NULL;
static lv_obj_t *label_time       = NULL;
static lv_obj_t *slider_progress  = NULL;
static lv_obj_t *label_btn_icon   = NULL;
static lv_timer_t *progress_timer = NULL;

// --- 声明 ---
static void scan_music_files(void);
static void app_music_event_cb(lv_event_t *e);
static void progress_timer_cb(lv_timer_t *timer);
static void close_app(void);

// 音频后端实现
void app_music_init_backend(void)
{
    if (is_engine_inited)
        return;

    ma_result result;
    // 初始化音频引擎 (自动连接 ALSA/PulseAudio)
    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS)
    {
        printf("Miniaudio: Failed to initialize audio engine.\n");
        return;
    }

    // 设置音量
    ma_engine_set_volume(&engine, current_volume);
    is_engine_inited = true;
    printf("Miniaudio: Engine initialized.\n");
}

void music_play_file(const char *path)
{
    if (!is_engine_inited)
        return;

    // 如果已经在播放，先卸载上一首
    if (is_sound_loaded)
    {
        ma_sound_uninit(&sound);
        is_sound_loaded = false;
    }

    // 从文件加载声音 (流式播放，不一次性解压到内存)
    // MA_SOUND_FLAG_STREAM: 对长音乐必须用流模式，否则内存爆炸
    ma_result result = ma_sound_init_from_file(&engine, path, MA_SOUND_FLAG_STREAM, NULL, NULL, &sound);

    if (result != MA_SUCCESS)
    {
        printf("Miniaudio: Failed to load file: %s\n", path);
        return;
    }

    is_sound_loaded = true;
    ma_sound_start(&sound); // 立即播放
    printf("Miniaudio: Playing %s\n", path);
}

void music_pause(void)
{
    if (is_sound_loaded)
        ma_sound_stop(&sound);
}

void music_resume(void)
{
    if (is_sound_loaded)
        ma_sound_start(&sound);
}

void music_toggle(void)
{
    if (!is_sound_loaded)
        return;

    if (ma_sound_is_playing(&sound))
    {
        music_pause();
    }
    else
    {
        music_resume();
    }
}

music_state_t music_get_state(void)
{
    if (!is_sound_loaded)
        return MUSIC_STATE_STOPPED;
    if (ma_sound_is_playing(&sound))
        return MUSIC_STATE_PLAYING;
    return MUSIC_STATE_PAUSED;
}

uint32_t music_get_total_time(void)
{
    if (!is_sound_loaded)
        return 0;
    float len_seconds;
    ma_sound_get_length_in_seconds(&sound, &len_seconds);
    return (uint32_t)len_seconds;
}

uint32_t music_get_current_time(void)
{
    if (!is_sound_loaded)
        return 0;
    float cursor_seconds;
    ma_sound_get_cursor_in_seconds(&sound, &cursor_seconds);
    return (uint32_t)cursor_seconds;
}

int music_get_progress_permille(void)
{
    uint32_t total = music_get_total_time();
    if (total == 0)
        return 0;
    uint32_t current = music_get_current_time();
    return (int)((current * 1000) / total);
}

void app_music_deinit(void)
{
    if (is_sound_loaded)
    {
        ma_sound_uninit(&sound);
        is_sound_loaded = false;
    }
    if (is_engine_inited)
    {
        ma_engine_uninit(&engine);
        is_engine_inited = false;
    }
}

// UI 逻辑实现

static void scan_music_files(void)
{
    DIR *d;
    struct dirent *dir;
    file_count = 0;

    d = opendir(MUSIC_DIR_PATH);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_REG)
            { // 只看普通文件
                // 简单判断后缀
                if (strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".wav"))
                {
                    if (file_count < MAX_FILES)
                    {
                        snprintf(file_list[file_count], MAX_FNAME_LEN, "%s", dir->d_name);
                        file_count++;
                    }
                }
            }
        }
        closedir(d);
    }
    printf("Music App: Found %d songs.\n", file_count);
}

// 播放当前索引的歌曲
static void play_current_index(void)
{
    if (file_count == 0)
        return;

    // 循环播放逻辑
    if (current_file_idx >= file_count)
        current_file_idx = 0;
    if (current_file_idx < 0)
        current_file_idx = file_count - 1;

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", MUSIC_DIR_PATH, file_list[current_file_idx]);

    // 调用后端播放
    music_play_file(full_path);

    // 更新 UI 标题
    lv_label_set_text(label_title, file_list[current_file_idx]);
    // 更新按钮状态为暂停图标
    lv_label_set_text(label_btn_icon, LV_SYMBOL_PAUSE);
}

// 定时器回调：更新进度条和时间
static void progress_timer_cb(lv_timer_t *timer)
{
    if (music_get_state() == MUSIC_STATE_PLAYING)
    {
        // 更新 Slider
        int permille = music_get_progress_permille();
        lv_slider_set_value(slider_progress, permille, LV_ANIM_ON);

        // 更新时间文本 00:00 / 03:45
        uint32_t cur = music_get_current_time();
        uint32_t tot = music_get_total_time();
        lv_label_set_text_fmt(label_time, "%02d:%02d / %02d:%02d",
                              cur / 60, cur % 60,
                              tot / 60, tot % 60);
    }
    // 自动切歌逻辑（可选）：如果播放结束（当前时间接近总时间且状态停止）
    else if (is_sound_loaded && !ma_sound_is_playing(&sound) && music_get_current_time() >= music_get_total_time() - 1)
    {
        // 下一曲
        current_file_idx++;
        play_current_index();
    }
}

static void app_music_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());

        switch (key)
        {
            case LV_KEY_RIGHT: // Key 1: 下一曲
                current_file_idx++;
                play_current_index();
                break;

            case LV_KEY_LEFT: // Key 1 Long: 上一曲
                current_file_idx--;
                play_current_index();
                break;

            case LV_KEY_ENTER: // Key 2: 播放/暂停
                music_toggle();
                if (music_get_state() == MUSIC_STATE_PLAYING)
                    lv_label_set_text(label_btn_icon, LV_SYMBOL_PAUSE);
                else
                    lv_label_set_text(label_btn_icon, LV_SYMBOL_PLAY);
                break;

            case LV_KEY_ESC: // Key 2 Long: 退出
                close_app();
                break;
        }
    }
}

static void close_app(void)
{
    // 停止定时器
    if (progress_timer)
    {
        lv_timer_del(progress_timer);
        progress_timer = NULL;
    }
    // 停止音乐并清理后端
    app_music_deinit();

    // 销毁 UI
    if (main_cont)
    {
        lv_obj_del(main_cont);
        main_cont = NULL;
    }
    printf("Music App Closed.\n");
}

// 主入口
void app_music_init(void)
{
    // 1. 初始化后端
    app_music_init_backend();
    scan_music_files();

    // 2. 创建 UI
    main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, 320, 240);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x202020), 0); // 深灰背景
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 加入按键组
    lv_group_t *g = lv_group_get_default();
    if (g)
    {
        lv_group_add_obj(g, main_cont);
        lv_group_focus_obj(main_cont);
    }
    lv_obj_add_event_cb(main_cont, app_music_event_cb, LV_EVENT_KEY, NULL);

    // 封面 (可以用一个 FontAwesome 音乐图标代替)
    lv_obj_t *icon = lv_label_create(main_cont);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0); // 假设有大字体，没有就用默认
    lv_label_set_text(icon, LV_SYMBOL_AUDIO);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFD700), 0); // 金色

    // 歌曲标题
    label_title = lv_label_create(main_cont);
    lv_obj_align(label_title, LV_ALIGN_CENTER, 0, 10);
    lv_label_set_text(label_title, "No Music Found");
    lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
    lv_label_set_long_mode(label_title, LV_LABEL_LONG_SCROLL_CIRCULAR); // 滚动显示长文件名
    lv_obj_set_width(label_title, 280);

    // 进度条
    slider_progress = lv_slider_create(main_cont);
    lv_obj_set_width(slider_progress, 280);
    lv_obj_align(slider_progress, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_slider_set_range(slider_progress, 0, 1000); // 0 ~ 1000‰

    // 时间显示
    label_time = lv_label_create(main_cont);
    lv_obj_align_to(label_time, slider_progress, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_label_set_text(label_time, "00:00 / 00:00");
    lv_obj_set_style_text_color(label_time, lv_color_hex(0xAAAAAA), 0);

    // 播放状态图标 (辅助显示)
    label_btn_icon = lv_label_create(main_cont);
    lv_label_set_text(label_btn_icon, LV_SYMBOL_PLAY);
    lv_obj_align(label_btn_icon, LV_ALIGN_BOTTOM_MID, 0, -10);

    // 3. 启动定时器 (每500ms更新一次 UI)
    progress_timer = lv_timer_create(progress_timer_cb, 500, NULL);

    // 4. 自动播放第一首
    if (file_count > 0)
    {
        current_file_idx = 0;
        play_current_index();
    }
}