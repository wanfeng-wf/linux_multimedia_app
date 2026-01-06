#ifndef APP_VIDEO_H
#define APP_VIDEO_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*app_video_exit_cb_t)(void);

void app_video_init(app_video_exit_cb_t exit_cb);
void app_video_close(void);

#ifdef __cplusplus
}
#endif
#endif