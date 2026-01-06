#include "app_video.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

// FFmpeg Headers
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

// --- 配置 ---
#define VIDEO_DIR_PATH "/root/multimedia_app"
#define MAX_FILES      20
#define MAX_FNAME_LEN  256

// 屏幕参数
#define SCREEN_W       MY_DISP_HOR_RES
#define SCREEN_H       MY_DISP_VER_RES
#define SCREEN_PIX_FMT AV_PIX_FMT_RGB565 // LVGL 通常使用 RGB565

// --- 全局变量：文件列表 ---
static char file_list[MAX_FILES][MAX_FNAME_LEN];
static int file_count       = 0;
static int current_file_idx = 0;

// --- 全局变量：FFmpeg ---
static AVFormatContext *pFormatCtx = NULL;
static AVCodecContext *pCodecCtx   = NULL;
static const AVCodec *pCodec       = NULL;
static AVFrame *pFrame             = NULL;
static AVPacket *packet            = NULL;
static struct SwsContext *sws_ctx  = NULL;
static int videoStream             = -1;
static uint8_t *display_buffer     = NULL; // 解码后的 RGB 数据
static int scaled_w, scaled_h;             // 缩放后的视频尺寸
static int offset_x, offset_y;             // 居中偏移
static int screen_linesize;                // 屏幕一行字节数

// --- 全局变量：UI ---
static lv_obj_t *main_cont    = NULL;
static lv_obj_t *img_display  = NULL;
static lv_obj_t *label_info   = NULL;
static lv_timer_t *play_timer = NULL;
static lv_img_dsc_t img_dsc;
static bool is_playing = false;
static bool is_paused  = false;

// 退出回调
static app_video_exit_cb_t g_exit_cb = NULL;

// --- 函数声明 ---
static void scan_video_files(void);
static int open_video_file(const char *filepath);
static void close_video_file(void);
static void video_timer_cb(lv_timer_t *timer);
static void app_video_event_cb(lv_event_t *e);

/**
 * @brief 扫描视频目录，填充文件列表
 * 遍历指定目录，将所有 MP4 文件添加到 file_list 中
 */
static void scan_video_files(void)
{
    DIR *d;
    struct dirent *dir;
    file_count = 0;

    d = opendir(VIDEO_DIR_PATH);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_REG)
            {
                if (strstr(dir->d_name, ".mp4") || strstr(dir->d_name, ".MP4"))
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
    printf("Video App: Found %d videos.\n", file_count);
}

/**
 * @brief 关闭视频文件
 * 释放 FFmpeg 资源，停止播放定时器，重置状态
 */
static void close_video_file(void)
{
    // 停止定时器
    if (play_timer)
    {
        lv_timer_del(play_timer);
        play_timer = NULL;
    }

    // 释放 FFmpeg 资源
    if (sws_ctx)
    {
        sws_freeContext(sws_ctx);
        sws_ctx = NULL;
    }
    if (pFrame)
    {
        av_frame_free(&pFrame);
        pFrame = NULL;
    }
    if (packet)
    {
        av_packet_free(&packet);
        packet = NULL;
    }
    if (pCodecCtx)
    {
        avcodec_free_context(&pCodecCtx);
        pCodecCtx = NULL;
    }
    if (pFormatCtx)
    {
        avformat_close_input(&pFormatCtx);
        pFormatCtx = NULL;
    }
    if (display_buffer)
    {
        av_free(display_buffer);
        display_buffer = NULL;
    }

    is_playing = false;
}

/**
 * @brief 打开视频文件
 * 初始化 FFmpeg，打开视频文件，查找视频流
 */
static int open_video_file(const char *filepath)
{
    close_video_file(); // 先清理旧的

    // 1. 打开文件
    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0)
    {
        printf("Video: Could not open file %s\n", filepath);
        return -1;
    }

    // 2. 查找流信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
        return -1;

    // 3. 寻找视频流
    videoStream = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1)
        return -1;

    // 4. 查找并打开解码器
    AVCodecParameters *pCodecPar = pFormatCtx->streams[videoStream]->codecpar;
    pCodec                       = avcodec_find_decoder(pCodecPar->codec_id);
    if (!pCodec)
        return -1;

    pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pCodecPar);
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        return -1;

    // 5. 分配资源
    pFrame = av_frame_alloc();
    packet = av_packet_alloc();

    // 计算 RGB565 缓冲区大小
    int numBytes   = av_image_get_buffer_size(SCREEN_PIX_FMT, SCREEN_W, SCREEN_H, 1);
    display_buffer = (uint8_t *)av_malloc(numBytes);

    // 初始化缓冲区为黑色
    memset(display_buffer, 0, numBytes);

    // 6. 计算缩放 (保持长宽比 + 居中)
    int src_w     = pCodecCtx->width;
    int src_h     = pCodecCtx->height;
    float scale_w = (float)SCREEN_W / src_w;
    float scale_h = (float)SCREEN_H / src_h;
    float scale   = (scale_w < scale_h) ? scale_w : scale_h; // Decrease 模式

    scaled_w = (int)(src_w * scale);
    scaled_h = (int)(src_h * scale);
    // 确保偶数
    scaled_w &= ~1;
    scaled_h &= ~1;

    offset_x        = (SCREEN_W - scaled_w) / 2;
    offset_y        = (SCREEN_H - scaled_h) / 2;
    screen_linesize = av_image_get_linesize(SCREEN_PIX_FMT, SCREEN_W, 0);

    // 7. 初始化 SWS Context
    sws_ctx = sws_getContext(src_w, src_h, pCodecCtx->pix_fmt,
                             scaled_w, scaled_h, SCREEN_PIX_FMT,
                             SWS_BILINEAR, NULL, NULL, NULL);

    // 8. 配置 LVGL Image Descriptor
    img_dsc.header.always_zero = 0;
    img_dsc.header.w           = SCREEN_W;
    img_dsc.header.h           = SCREEN_H;
    img_dsc.data_size          = numBytes;
    img_dsc.header.cf          = LV_IMG_CF_TRUE_COLOR;
    img_dsc.data               = display_buffer;

    lv_img_set_src(img_display, &img_dsc);

    // 9. 计算帧率并启动定时器
    double fps = av_q2d(pFormatCtx->streams[videoStream]->avg_frame_rate);
    if (fps < 1.0)
        fps = 30.0; // 默认值
    int interval_ms = (int)(1000.0 / fps);

    printf("Video Info: %dx%d @ %.2f fps. Timer: %d ms\n", src_w, src_h, fps, interval_ms);

    play_timer = lv_timer_create(video_timer_cb, interval_ms, NULL);
    is_playing = true;
    is_paused  = false;

    return 0;
}

/**
 * @brief 视频播放定时器回调
 * 读取视频帧，解码显示，支持暂停和循环播放
 */
static void video_timer_cb(lv_timer_t *timer)
{
    if (!is_playing || is_paused)
        return;

    int ret;
    while (av_read_frame(pFormatCtx, packet) >= 0)
    {
        if (packet->stream_index == videoStream)
        {
            ret = avcodec_send_packet(pCodecCtx, packet);
            if (ret < 0)
            {
                av_packet_unref(packet);
                continue;
            }

            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret == 0)
            {
                // 1. 清空背景 (Black Pad)
                // 优化：如果视频铺满全屏，可以省去这一步；如果不满，则必须清空
                if (scaled_w < SCREEN_W || scaled_h < SCREEN_H)
                {
                    memset(display_buffer, 0, img_dsc.data_size);
                }

                // 2. 构造目标指针 (指向居中位置)
                uint8_t *dst_data[4];
                int dst_linesize[4];

                dst_linesize[0] = screen_linesize;
                // 指针偏移算法：Buffer Start + Y偏移行 + X偏移像素(2字节/像素)
                dst_data[0] = display_buffer + (offset_y * screen_linesize) + (offset_x * 2);

                // 3. 缩放转换
                sws_scale(sws_ctx, (const uint8_t *const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          dst_data, dst_linesize);

                // 4. 刷新 UI
                lv_obj_invalidate(img_display);

                av_packet_unref(packet);
                return; // 解码显示一帧后退出，等待下一次定时器
            }
        }
        av_packet_unref(packet);
    }

    // 循环播放逻辑
    avio_seek(pFormatCtx->pb, 0, SEEK_SET);
    av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
}

/**
 * @brief 播放当前索引的视频文件
 * 关闭当前视频，打开新文件，更新 UI 标题
 */
static void play_current_index(void)
{
    if (file_count == 0)
        return;
    if (current_file_idx >= file_count)
        current_file_idx = 0;
    if (current_file_idx < 0)
        current_file_idx = file_count - 1;

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", VIDEO_DIR_PATH, file_list[current_file_idx]);

    // 更新 UI 标题
    lv_label_set_text(label_info, file_list[current_file_idx]);

    open_video_file(full_path);
}

/**
 * @brief 视频应用按键事件处理
 * 处理播放、暂停、切换视频等操作
 */
static void app_video_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());
        switch (key)
        {
            case LV_KEY_RIGHT: // Next Video
                current_file_idx++;
                play_current_index();
                break;
            case LV_KEY_LEFT: // Prev Video
                current_file_idx--;
                play_current_index();
                break;
            case LV_KEY_ENTER: // Pause/Resume
                is_paused = !is_paused;
                lv_label_set_text(label_info, is_paused ? "PAUSED" : file_list[current_file_idx]);
                break;
            case LV_KEY_ESC: // Quit
                if (g_exit_cb) // 主程序关闭
                    g_exit_cb();
                break;
        }
    }
}

/**
 * @brief 初始化视频应用
 * 创建 UI 容器、扫描视频文件、初始化 FFmpeg
 */
void app_video_init(app_video_exit_cb_t exit_cb)
{
    g_exit_cb = exit_cb;
    
    // 1. 扫描文件
    scan_video_files();

    // 2. 创建 UI
    main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(main_cont, lv_color_black(), 0);
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_group_t *g = lv_group_get_default();
    if (g)
    {
        lv_group_add_obj(g, main_cont);
        lv_group_focus_obj(main_cont);
    }
    lv_obj_add_event_cb(main_cont, app_video_event_cb, LV_EVENT_KEY, NULL);

    // 图片控件 (用于显示视频帧)
    img_display = lv_img_create(main_cont);
    lv_obj_center(img_display);

    // 信息标签
    label_info = lv_label_create(main_cont);
    lv_obj_align(label_info, LV_ALIGN_TOP_MID, 0, -5);
    lv_obj_set_style_text_color(label_info, lv_color_white(), 0);
    lv_label_set_text(label_info, "Loading Video...");

    // 3. 播放第一个
    if (file_count > 0)
    {
        current_file_idx = 0;
        play_current_index();
    }
    else
    {
        lv_label_set_text(label_info, "No MP4 Files Found!");
    }
}

/**
 * @brief 关闭视频应用
 * 释放所有资源，关闭视频文件，删除 UI 容器
 */
void app_video_close(void)
{
    close_video_file();

    if (main_cont)
    {
        lv_obj_del(main_cont);
        main_cont = NULL;
    }
    printf("Video App Closed.\n");
}