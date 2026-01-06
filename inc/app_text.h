#ifndef APP_TEXT_H
#define APP_TEXT_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*app_text_exit_cb_t)(void);

void app_text_init(app_text_exit_cb_t exit_cb);
void app_text_close(void);

#ifdef __cplusplus
}
#endif

#endif