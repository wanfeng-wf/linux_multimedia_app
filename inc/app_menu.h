#ifndef APP_MENU_H
#define APP_MENU_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

// 应用 ID 定义
typedef enum
{
    APP_ID_IMAGE = 0,
    APP_ID_TEXT,
    APP_ID_MUSIC,
    APP_ID_VIDEO,
    APP_ID_NONE
} app_id_t;

// 定义回调函数类型：当用户在菜单中选择了一个应用时调用
// 参数 app_id: 用户选择的应用 ID
typedef void (*app_menu_select_cb_t)(app_id_t app_id);

// 初始化菜单
// select_cb: 传入的回调函数
void app_menu_init(app_menu_select_cb_t select_cb);

// 关闭菜单
void app_menu_close(void);

#ifdef __cplusplus
}
#endif

#endif // APP_MENU_H