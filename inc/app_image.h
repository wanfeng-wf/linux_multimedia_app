#ifndef APP_IMAGE_H
#define APP_IMAGE_H

#ifdef __cplusplus
extern "C"
{
#endif

// 定义退出回调函数类型
typedef void (*app_image_exit_cb_t)(void);

// 初始化：传入退出时的回调函数
void app_image_init(app_image_exit_cb_t exit_cb);

// 关闭应用：销毁资源
void app_image_close(void);

#ifdef __cplusplus
}
#endif

#endif // APP_IMAGE_H