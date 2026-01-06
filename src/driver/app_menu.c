#include "app_menu.h"
#include "lvgl.h"
#include <stdio.h>

// --- 静态变量 ---
static lv_obj_t *main_cont              = NULL;
static app_menu_select_cb_t g_select_cb = NULL;
static lv_group_t *menu_group           = NULL;

/**
 * @brief 菜单按键点击事件处理
 * 当用户点击菜单项时触发，将对应的 App ID 通知主程序
 */
static void menu_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        // 获取之前绑定的 App ID
        // 通过 (intptr_t) 强转回来
        app_id_t id = (app_id_t)(intptr_t)lv_event_get_user_data(e);

        printf("Menu: Selected App ID %d\n", id);

        // 通知主程序
        if (g_select_cb)
        {
            g_select_cb(id);
        }
    }
}

/**
 * @brief 菜单导航事件处理
 * 当用户按下 RIGHT/LEFT 键时，手动切换焦点到下一个/上一个菜单项
 */
static void menu_navigation_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());
        // printf("%s:%s:%d: Key pressed %d\n", __FILE__, __FUNCTION__, __LINE__, key);
        if (key == LV_KEY_RIGHT)
        {
            lv_group_focus_next(menu_group); // 手动切到下一个
        }
        else if (key == LV_KEY_LEFT)
        {
            lv_group_focus_prev(menu_group); // 手动切到上一个
        }
    }
}

/**
 * @brief 创建一个菜单选项卡
 * 每个选项卡包含图标、文字和点击事件
 */
static void create_menu_item(lv_obj_t *parent, const char *icon, const char *text, app_id_t id)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 130, 90);                             // 卡片大小
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x404040), 0); // 深灰背景
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);            // 垂直布局：上图下文
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 直接在这里绑定事件和 ID
    // (void *)(intptr_t)id 是将整数 ID 伪装成指针传给 user_data
    lv_obj_add_event_cb(btn, menu_event_handler, LV_EVENT_CLICKED, (void *)(intptr_t)id);

    // 绑定导航事件 (让按钮能响应左右键切焦点)
    lv_obj_add_event_cb(btn, menu_navigation_cb, LV_EVENT_KEY, NULL);

    // 图标
    lv_obj_t *lbl_icon = lv_label_create(btn);
    // 确保你在 lv_conf.h 中把 LV_FONT_MONTSERRAT_28 置为 1，否则这里会报错
    // 如果不想改 lv_conf.h，可以暂时改成 &lv_font_montserrat_14
    lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_28, 0);
    lv_label_set_text(lbl_icon, icon);

    // 文字
    lv_obj_t *lbl_text = lv_label_create(btn);
    lv_label_set_text(lbl_text, text);

    // 添加到组，支持按键导航
    if (menu_group)
    {
        lv_group_add_obj(menu_group, btn);
    }
}

/**
 * @brief 初始化应用菜单
 * 创建主容器、网格布局、4 个应用图标
 */
void app_menu_init(app_menu_select_cb_t select_cb)
{
    g_select_cb = select_cb;

    // 1. 创建主容器
    main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, 320, 240);
    lv_obj_set_pos(main_cont, 0, 0);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x202020), 0);
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 标题栏
    lv_obj_t *title = lv_label_create(main_cont);
    lv_label_set_text(title, "Menu");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFD700), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // 2. 创建内容容器 (Flex 网格)
    lv_obj_t *grid_cont = lv_obj_create(main_cont);

    // 容器尺寸撑满下方区域
    lv_obj_set_size(grid_cont, 320, 200);
    lv_obj_align(grid_cont, LV_ALIGN_BOTTOM_MID, 0, 10);

    // 清除内边距，防止空间不够
    lv_obj_set_style_pad_all(grid_cont, 10, 0);    // 四周留10px边距
    lv_obj_set_style_pad_row(grid_cont, 10, 0);    // 行间距
    lv_obj_set_style_pad_column(grid_cont, 10, 0); // 列间距

    lv_obj_set_style_bg_opa(grid_cont, 0, 0);
    lv_obj_set_style_border_width(grid_cont, 0, 0);
    lv_obj_clear_flag(grid_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 设置 Flex 布局
    lv_obj_set_flex_flow(grid_cont, LV_FLEX_FLOW_ROW_WRAP);
    // 使用 CENTER 对齐，不用 SPACE_EVENLY，防止算错
    lv_obj_set_flex_align(grid_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 3. 准备按键组
    menu_group = lv_group_get_default();
    if (!menu_group)
    {
        menu_group = lv_group_create();
        lv_group_set_default(menu_group);
    }
    lv_group_remove_all_objs(menu_group);

    // 4. 创建 4 个应用图标
    // 相册
    create_menu_item(grid_cont, LV_SYMBOL_IMAGE, "Gallery", APP_ID_IMAGE);
    // 小说
    create_menu_item(grid_cont, LV_SYMBOL_FILE, "Reader", APP_ID_TEXT);
    // 音乐
    create_menu_item(grid_cont, LV_SYMBOL_AUDIO, "Music", APP_ID_MUSIC);
    // 视频
    create_menu_item(grid_cont, LV_SYMBOL_VIDEO, "Video", APP_ID_VIDEO);

    // 聚焦第一个
    if (lv_obj_get_child_cnt(grid_cont) > 0)
    {
        lv_group_focus_obj(lv_obj_get_child(grid_cont, 0));
    }
}

/**
 * @brief 关闭应用菜单
 * 删除主容器和按键组
 */
void app_menu_close(void)
{
    if (main_cont)
    {
        lv_obj_del(main_cont);
        main_cont = NULL;
    }
    if (menu_group)
    {
        lv_group_remove_all_objs(menu_group);
    }
}