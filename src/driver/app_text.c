#include "app_text.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- 策略配置 ---
#define FONT_PATH "/root/multimedia_app/font.ttf"
#define BOOK_PATH "/root/multimedia_app/novel.txt"

// 屏幕与排版参数
#define SCREEN_W       320
#define SCREEN_H       240
#define FONT_SIZE      14                       // 字体大小 px
#define LINE_SPACE     4                        // 行间距
#define HEADER_H       20                       // 顶部状态栏高度
#define BODY_H         (SCREEN_H - HEADER_H)    // 正文高度
#define LINE_H         (FONT_SIZE + LINE_SPACE) // 行高
#define MAX_LINES      (BODY_H / LINE_H)        // 理论最大行数
#define CHARS_PER_LINE (SCREEN_W / FONT_SIZE)   // 全角汉字数
#define HALF_WIDTH_MAX (CHARS_PER_LINE * 2)     // 半角单位宽度

// 缓冲区大小 (足够容纳一页的 UTF-8 编码字符)
// 11行 * 20字 * 3字节 + 换行符等，1KB 足够
#define PAGE_BUF_SIZE 2048
#define MAX_HISTORY   500 // 记录 500 页的历史，用于翻页

// --- 全局变量 ---
static lv_obj_t *main_cont    = NULL;
static lv_obj_t *label_header = NULL;
static lv_obj_t *label_body   = NULL;
static lv_style_t style_body; // 专用样式控制行高

static lv_ft_info_t font_info;
static lv_font_t *my_font = NULL;

static FILE *book_file      = NULL;
static long file_total_size = 0;
static int g_max_lines      = MAX_LINES; // 实际计算出的最大行数

// 翻页历史管理
static long page_history[MAX_HISTORY];
static int page_history_idx  = 0; // 当前页码索引 (0 = 第1页)
static long next_page_offset = 0; // 下一页的文件偏移量

// --- 函数声明 ---
static void render_page(void);
static void process_layout(long start_offset, char *out_buf, long *new_offset);
static void close_app(void);
static void app_text_event_cb(lv_event_t *e);

/**
 * @brief 核心排版引擎 (Strategy 3 & 4)
 * 读取文件，清洗数据，计算换行，生成一页的显示字符串
 * * @param start_offset 从文件的哪个位置开始读
 * @param out_buf      输出缓冲区
 * @param new_offset   [out] 排版结束后，文件指针到了哪里 (用于下一页)
 */
static void process_layout(long start_offset, char *out_buf, long *new_offset)
{
    if (!book_file)
        return;

    fseek(book_file, start_offset, SEEK_SET);

    int current_line     = 0;
    int current_width    = 0;
    int buf_idx          = 0;
    int empty_line_count = 0;
    int has_content      = 0; // 【新增】标记当前页是否有实质内容

    // 读取用的临时变量
    int ch;
    unsigned char utf8_buf[6];

    // 清空缓冲区
    memset(out_buf, 0, PAGE_BUF_SIZE);

    while (current_line < g_max_lines)
    {
        long char_start_pos = ftell(book_file);
        ch                  = fgetc(book_file);

        if (ch == EOF)
            break;

        // 1. 过滤回车符
        if (ch == '\r')
            continue;

        // 2. 段落及章节处理
        if (ch == '\n')
        {
            // 判断是否是空行
            if (current_width == 0)
            {
                empty_line_count++;
            }
            else
            {
                empty_line_count = 0;
            }

            // ---【核心修改开始】---
            // 检测到连续空行 > 2 (即章节分隔)
            if (empty_line_count > 2)
            {
                // 1. 吞噬后续所有空白字符，直到找到下一章的第一个字
                int next_c;
                while (1)
                {
                    long peek_pos = ftell(book_file);
                    next_c        = fgetc(book_file);
                    if (next_c == EOF)
                        break;
                    // 跳过换行、回车、空格、制表符
                    if (next_c != '\n' && next_c != '\r' && next_c != ' ' && next_c != '\t')
                    {
                        // 找到了正文，回退指针供后续读取
                        fseek(book_file, peek_pos, SEEK_SET);
                        break;
                    }
                }

                // 2. 决定是结束当前页，还是重置当前页
                if (has_content == 0)
                {
                    // 【关键修复】如果当前页还没写过字（说明空行在页首），
                    // 则丢弃刚才读到的换行符，重置状态，继续在“这一页”显示新章节
                    buf_idx          = 0;
                    current_line     = 0;
                    current_width    = 0;
                    empty_line_count = 0;

                    // 注意：这里不需要 fseek，因为上面的吞噬逻辑已经把文件指针移到了新章节开头
                    continue;
                }
                else
                {
                    // 如果当前页已经有上章结尾的内容，则结束本页，让新章节在下一页显示
                    break;
                }
            }
            // ---【核心修改结束】---

            // 正常换行处理
            if (current_line < g_max_lines)
            {
                out_buf[buf_idx++] = '\n';
                current_line++;
                current_width = 0;
            }
            continue;
        }

        // 3. 标记内容状态
        // 只要读到了非换行符，就认为当前页有内容了
        has_content = 1;
        // 遇到字符，重置空行计数
        empty_line_count = 0;

        // 4. 字符宽度计算
        int char_width    = 1;
        int bytes_to_read = 0;

        utf8_buf[0] = (unsigned char)ch;

        if (ch < 0x80)
        {
            bytes_to_read = 0;
            char_width    = 1;
        }
        else if ((ch & 0xE0) == 0xC0)
        {
            bytes_to_read = 1;
            char_width    = 2;
        }
        else if ((ch & 0xF0) == 0xE0)
        {
            bytes_to_read = 2;
            char_width    = 2;
        }
        else if ((ch & 0xF8) == 0xF0)
        {
            bytes_to_read = 3;
            char_width    = 2;
        }

        for (int i = 0; i < bytes_to_read; i++)
        {
            int next_c = fgetc(book_file);
            if (next_c == EOF)
                break;
            utf8_buf[1 + i] = (unsigned char)next_c;
        }

        // 5. 软换行处理
        if (current_width + char_width > HALF_WIDTH_MAX)
        {
            out_buf[buf_idx++] = '\n';
            current_line++;
            current_width = 0;
            // 软换行不算空行，重置计数
            empty_line_count = 0;

            if (current_line >= g_max_lines)
            {
                fseek(book_file, char_start_pos, SEEK_SET);
                break;
            }
        }

        // 6. 输出字符
        for (int i = 0; i <= bytes_to_read; i++)
        {
            out_buf[buf_idx++] = utf8_buf[i];
        }
        current_width += char_width;
    }

    out_buf[buf_idx] = '\0';
    *new_offset      = ftell(book_file);
}

/**
 * @brief 渲染当前页 (Strategy 6)
 */
static void render_page(void)
{
    if (!book_file)
        return;

    // 1. 获取当前页的起始偏移量
    long start_offset = page_history[page_history_idx];

    // 2. 排版计算
    char display_buf[PAGE_BUF_SIZE];
    process_layout(start_offset, display_buf, &next_page_offset);

    // 3. 更新正文 UI
    lv_label_set_text(label_body, display_buf);

    // 4. 更新顶部 Header (进度信息)
    long percent = 0;
    if (file_total_size > 0)
    {
        percent = (next_page_offset * 100) / file_total_size;
    }
    // 格式：(页码) 进度%
    lv_label_set_text_fmt(label_header, "Page %d  -  %ld%%", page_history_idx + 1, percent);
}

/**
 * @brief 初始化应用
 */
void app_text_init(void)
{
    // --- 1. 资源加载 ---
    if (my_font == NULL)
    {
        font_info.name   = FONT_PATH;
        font_info.weight = FONT_SIZE;
        font_info.style  = FT_FONT_STYLE_NORMAL;
        if (lv_ft_font_init(&font_info))
        {
            my_font = font_info.font;
        }
        else
        {
            printf("Error: Failed to load font: %s\n", FONT_PATH);
            return;
        }
    }

    // 动态计算能放几行
    int real_font_h = lv_font_get_line_height(my_font);
    g_max_lines     = BODY_H / (real_font_h + LINE_SPACE);
    printf("Layout Debug: BodyH=%d, FontH=%d, MaxLines=%d\n", BODY_H, real_font_h, g_max_lines);

    // 打开文件
    book_file = fopen(BOOK_PATH, "r");
    if (!book_file)
    {
        printf("Error: Cannot open %s\n", BOOK_PATH);
        return;
    }
    fseek(book_file, 0, SEEK_END);
    file_total_size = ftell(book_file);
    rewind(book_file);

    // 初始化历史记录
    page_history[0]  = 0;
    page_history_idx = 0;

    // --- 2. 界面布局 (Strategy 5) ---
    // 2.1 主容器 (黑色背景，护眼)
    main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0xFFFFF0), 0); // 米黄色背景
    lv_obj_set_style_pad_all(main_cont, 0, 0);                       // 无内边距
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 2.2 顶部 Header
    label_header = lv_label_create(main_cont);
    lv_obj_set_size(label_header, SCREEN_W, HEADER_H);
    lv_obj_align(label_header, LV_ALIGN_TOP_LEFT, 5, 2);                  // 左上角，微调
    lv_obj_set_style_text_font(label_header, &lv_font_montserrat_14, 0);  // 用内置小字体
    lv_obj_set_style_text_color(label_header, lv_color_hex(0x666666), 0); // 灰色文字

    // 2.3 正文 Body
    label_body = lv_label_create(main_cont);
    lv_obj_set_width(label_body, SCREEN_W);
    lv_obj_align(label_body, LV_ALIGN_TOP_LEFT, 0, HEADER_H); // 紧接 Header 下方

    // 设置正文样式
    lv_style_init(&style_body);
    lv_style_set_text_font(&style_body, my_font);
    lv_style_set_text_color(&style_body, lv_color_black());
    lv_style_set_text_line_space(&style_body, LINE_SPACE); // 策略 2: 4px 行间距
    lv_obj_add_style(label_body, &style_body, 0);

    // 重要：排版引擎已经手动处理了换行，所以告诉 LVGL 不要乱动
    // 但为了保险，还是保留 WRAP，以防某个超长单词溢出
    lv_label_set_long_mode(label_body, LV_LABEL_LONG_WRAP);

    // --- 3. 事件绑定 ---
    lv_group_t *g = lv_group_get_default();
    if (g)
    {
        lv_group_add_obj(g, main_cont);
        lv_group_focus_obj(main_cont);
    }
    lv_obj_add_event_cb(main_cont, app_text_event_cb, LV_EVENT_KEY, NULL);

    // --- 4. 首次渲染 ---
    render_page();
}

static void app_text_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());
        switch (key)
        {
            case LV_KEY_RIGHT: // 下一页
                if (next_page_offset < file_total_size && page_history_idx < MAX_HISTORY - 1)
                {
                    // 记录下一页的起始位置
                    page_history_idx++;
                    page_history[page_history_idx] = next_page_offset;
                    render_page();
                }
                break;
            case LV_KEY_LEFT: // 上一页
                if (page_history_idx > 0)
                {
                    page_history_idx--;
                    // 上一页的 offset 已经在历史栈里了，直接取
                    render_page();
                }
                break;
            case LV_KEY_ESC:
                close_app();
                break;
        }
    }
}

static void close_app(void)
{
    if (book_file)
    {
        fclose(book_file);
        book_file = NULL;
    }
    if (main_cont)
    {
        lv_obj_del(main_cont);
        main_cont = NULL;
    }
    // lv_ft_font_destroy(my_font); // 如果需要释放字体
    printf("App Text Closed.\n");
}