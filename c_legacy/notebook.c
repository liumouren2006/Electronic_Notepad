/*
 * 墨记 · 电子记事本（C 语言版）
 * 课程设计《电子记事本的实现》基础要求实现：
 *   - 单向链表按优先级组织事件，插入时后续优先级自动顺延
 *   - UTF-8 文本文件持久化（notes.dat）
 *   - 菜单 + 循环，一次运行可做多项管理
 *   - 删除 / 修改 / 按日期查询 / 关键字搜索 / 显示
 *   - 启动时若当天已有安排，蜂鸣提醒
 *
 * 编译：
 *   gcc -std=c89 -Wall -Wextra -o notebook notebook.c
 *   （Windows 下可用 MinGW；VC++6.0 请把本文件另存为 GBK 编码再编译）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_TITLE 128
#define MAX_NOTE 512
#define MAX_DUE 20
#define MAX_LINE 1024
#define DATA_FILE "notes.dat"

typedef struct Event {
    int id;
    int priority;              /* 1 起始，由链表顺序维护 */
    int done;
    char title[MAX_TITLE];
    char note[MAX_NOTE];
    char due[MAX_DUE];         /* "YYYY-MM-DD HH:MM"，空串表示无期限 */
    char created[MAX_DUE];
    struct Event *next;
} Event;

static Event *g_head = NULL;
static int g_eof = 0;
static const char *g_file = DATA_FILE;

static void print_today_brief(void);

/* ------------------------------------------------------------ 工具函数 */
static void safe_copy(char *dst, int size, const char *src)
{
    if (size <= 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    {
        size_t n = strlen(src);
        if (n >= (size_t)size) {
            n = (size_t)size - 1;
        }
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
}

static void strip_newline(char *s)
{
    size_t len;
    if (s == NULL) {
        return;
    }
    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

/* 去掉会破坏文本存储格式的字符 */
static void sanitize(char *s)
{
    int i;
    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] == '|' || s[i] == '\n' || s[i] == '\r') {
            s[i] = ' ';
        }
    }
}

static void read_line(char *buf, int size)
{
    if (fgets(buf, size, stdin) == NULL) {
        buf[0] = '\0';
        g_eof = 1;
        return;
    }
    strip_newline(buf);
}

static int read_int(const char *prompt)
{
    char buf[64];
    if (prompt != NULL) {
        printf("%s", prompt);
        fflush(stdout);
    }
    read_line(buf, sizeof(buf));
    return atoi(buf);
}

static void today_str(char *buf, int size)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt != NULL) {
        strftime(buf, (size_t)size, "%Y-%m-%d", lt);
    } else {
        safe_copy(buf, size, "1970-01-01");
    }
}

static void now_str(char *buf, int size)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt != NULL) {
        strftime(buf, (size_t)size, "%Y-%m-%d %H:%M", lt);
    } else {
        safe_copy(buf, size, "1970-01-01 00:00");
    }
}

/* 校验 "YYYY-MM-DD[ HH:MM]"；date_only 为真时允许只有日期 */
static int valid_due(const char *text, int date_only)
{
    int y = 0, m = 0, d = 0, hh = 0, mm = 0;
    int n;
    if (text == NULL || strlen(text) < 10) {
        return 0;
    }
    n = sscanf(text, "%d-%d-%d %d:%d", &y, &m, &d, &hh, &mm);
    if (n < 3) {
        return 0;
    }
    if (y < 1970 || y > 2999 || m < 1 || m > 12 || d < 1 || d > 31) {
        return 0;
    }
    if (n >= 5) {
        if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
            return 0;
        }
    }
    if (date_only) {
        return 1;
    }
    return n >= 5;
}

static void read_due(char *buf, int size, const char *prompt)
{
    char tmp[MAX_LINE];
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        read_line(tmp, sizeof(tmp));
        if (g_eof) {
            buf[0] = '\0';
            return;
        }
        if (tmp[0] == '\0') {
            buf[0] = '\0';
            return;
        }
        if (tmp[0] == '-' && tmp[1] == '\0') {
            buf[0] = '\0';
            return;
        }
        if (strlen(tmp) >= 10 && strchr(tmp, ':') == NULL) {
            char full[MAX_DUE];
            if (!valid_due(tmp, 1)) {
                printf("  日期格式不对，请用 YYYY-MM-DD 或 YYYY-MM-DD HH:MM。\n");
                continue;
            }
            sprintf(full, "%s 00:00", tmp);
            safe_copy(buf, size, full);
            return;
        }
        if (!valid_due(tmp, 0)) {
            printf("  日期格式不对，请用 YYYY-MM-DD HH:MM。\n");
            continue;
        }
        safe_copy(buf, size, tmp);
        return;
    }
}

/* ------------------------------------------------------------ 链表操作 */
static int list_count(Event *head)
{
    int n = 0;
    while (head != NULL) {
        n++;
        head = head->next;
    }
    return n;
}

static void list_reindex(Event *head)
{
    int p = 1;
    while (head != NULL) {
        head->priority = p++;
        head = head->next;
    }
}

static Event *list_find(Event *head, int id)
{
    while (head != NULL) {
        if (head->id == id) {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

/* 在 pos（1 起始，允许 len+1）处插入；原 pos 及之后优先级顺延 */
static void list_insert_at(Event **head, Event *ev, int pos)
{
    Event *prev = NULL;
    Event *cur = *head;
    int i = 1;
    if (pos < 1) {
        pos = 1;
    }
    while (cur != NULL && i < pos) {
        prev = cur;
        cur = cur->next;
        i++;
    }
    ev->next = cur;
    if (prev == NULL) {
        *head = ev;
    } else {
        prev->next = ev;
    }
    list_reindex(*head);
}

/* 把节点从链表摘下但不释放，用于修改优先级 */
static void list_unlink(Event **head, Event *target)
{
    Event *prev = NULL;
    Event *cur = *head;
    while (cur != NULL) {
        if (cur == target) {
            if (prev == NULL) {
                *head = cur->next;
            } else {
                prev->next = cur->next;
            }
            target->next = NULL;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

static int list_remove(Event **head, int id)
{
    Event *prev = NULL;
    Event *cur = *head;
    while (cur != NULL) {
        if (cur->id == id) {
            if (prev == NULL) {
                *head = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            list_reindex(*head);
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

static void list_clear(Event **head)
{
    Event *cur = *head;
    while (cur != NULL) {
        Event *next = cur->next;
        free(cur);
        cur = next;
    }
    *head = NULL;
}

static int next_id(Event *head)
{
    int max_id = 0;
    while (head != NULL) {
        if (head->id > max_id) {
            max_id = head->id;
        }
        head = head->next;
    }
    return max_id + 1;
}

static Event *event_new(void)
{
    Event *ev = (Event *)malloc(sizeof(Event));
    if (ev == NULL) {
        printf("内存不足！\n");
        exit(1);
    }
    memset(ev, 0, sizeof(Event));
    return ev;
}

/* ------------------------------------------------------------ 文件读写 */
static int split_fields(char *line, char fields[][MAX_NOTE], int max_fields)
{
    int n = 0;
    char *start = line;
    char *bar;
    for (;;) {
        if (n >= max_fields) {
            break;
        }
        bar = strchr(start, '|');
        if (bar == NULL) {
            safe_copy(fields[n], MAX_NOTE, start);
            n++;
            break;
        }
        *bar = '\0';
        safe_copy(fields[n], MAX_NOTE, start);
        n++;
        start = bar + 1;
    }
    return n;
}

static int load_file(Event **head, const char *path)
{
    FILE *fp;
    char line[MAX_LINE];
    char fields[6][MAX_NOTE];
    int count = 0;
    int lineno = 0;
    Event *tail = NULL;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return 0;
    }
    strip_newline(line);
    if (strncmp(line, "# MOJI-C", 8) != 0) {
        printf("警告：%s 不是墨记数据文件，尝试按行读取。\n", path);
        rewind(fp);
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        Event *ev;
        int n;
        lineno++;
        strip_newline(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        n = split_fields(line, fields, 6);
        if (n < 5) {
            printf("警告：第 %d 行字段不足，已跳过。\n", lineno);
            continue;
        }
        ev = event_new();
        ev->id = atoi(fields[0]);
        ev->priority = atoi(fields[1]);
        safe_copy(ev->due, MAX_DUE, fields[2]);
        ev->done = atoi(fields[3]);
        safe_copy(ev->title, MAX_TITLE, fields[4]);
        safe_copy(ev->note, MAX_NOTE, n >= 6 ? fields[5] : "");
        if (ev->id <= 0 || ev->title[0] == '\0') {
            printf("警告：第 %d 行数据无效，已跳过。\n", lineno);
            free(ev);
            continue;
        }
        if (*head == NULL) {
            *head = ev;
        } else {
            tail->next = ev;
        }
        tail = ev;
        count++;
    }
    fclose(fp);
    list_reindex(*head);
    return count;
}

static int save_file(Event *head, const char *path)
{
    FILE *fp;
    Event *cur;
    char tmp_path[MAX_LINE];
    sprintf(tmp_path, "%s.tmp", path);
    fp = fopen(tmp_path, "wb");
    if (fp == NULL) {
        printf("保存失败：无法写入 %s（磁盘或权限问题）。\n", tmp_path);
        return 0;
    }
    fprintf(fp, "# MOJI-C 1\n");
    for (cur = head; cur != NULL; cur = cur->next) {
        fprintf(fp, "%d|%d|%s|%d|%s|%s\n",
                cur->id, cur->priority, cur->due, cur->done,
                cur->title, cur->note);
    }
    if (fclose(fp) != 0) {
        printf("保存失败：写入 %s 出错。\n", tmp_path);
        return 0;
    }
    remove(path);
    if (rename(tmp_path, path) != 0) {
        printf("保存失败：无法替换 %s。\n", path);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------ 展示 */
static void print_event(const Event *ev)
{
    char state[32];
    if (ev->done) {
        safe_copy(state, sizeof(state), "已完成");
    } else if (ev->due[0] == '\0') {
        safe_copy(state, sizeof(state), "无期限");
    } else {
        char today[16];
        today_str(today, sizeof(today));
        if (strncmp(ev->due, today, 10) < 0) {
            safe_copy(state, sizeof(state), "已逾期");
        } else if (strncmp(ev->due, today, 10) == 0) {
            safe_copy(state, sizeof(state), "今天");
        } else {
            safe_copy(state, sizeof(state), ev->due);
        }
    }
    printf("  P%-2d %s #%-3d %-16s %s",
           ev->priority, ev->done ? "[x]" : "[ ]", ev->id,
           state, ev->title);
    if (ev->note[0] != '\0') {
        printf("  // %s", ev->note);
    }
    printf("\n");
}

static void print_list(Event *head, const char *title)
{
    int n = 0;
    printf("\n--- %s ---\n", title);
    while (head != NULL) {
        print_event(head);
        head = head->next;
        n++;
    }
    if (n == 0) {
        printf("  （没有符合条件的记事）\n");
    }
    printf("--- 共 %d 条 ---\n", n);
}

static void beep_reminder(Event *head)
{
    char today[16];
    int n = 0;
    today_str(today, sizeof(today));
    while (head != NULL) {
        if (!head->done && strncmp(head->due, today, 10) == 0) {
            n++;
        }
        head = head->next;
    }
    if (n > 0) {
#ifdef _WIN32
        Beep(1000, 280);
        Sleep(80);
        Beep(1200, 220);
#else
        printf("\a");
        fflush(stdout);
#endif
        printf("🔔 提醒：今天已有 %d 项安排，别忘了查看。\n", n);
    }
}

/* ------------------------------------------------------------ 业务操作 */
static void do_add(int insert_mode)
{
    Event *ev = event_new();
    char buf[MAX_LINE];
    int total = list_count(g_head);
    int pos = total + 1;

    printf("\n请输入标题：");
    read_line(buf, sizeof(buf));
    if (g_eof || buf[0] == '\0') {
        free(ev);
        printf("标题不能为空，已取消。\n");
        return;
    }
    sanitize(buf);
    safe_copy(ev->title, MAX_TITLE, buf);

    read_due(ev->due, MAX_DUE, "截止时间（YYYY-MM-DD HH:MM，回车跳过）：");

    printf("备注（可回车跳过）：");
    read_line(buf, sizeof(buf));
    sanitize(buf);
    safe_copy(ev->note, MAX_NOTE, buf);

    if (insert_mode) {
        pos = read_int("插入到第几位？（原该位置及之后自动后移）：");
        if (pos < 1) {
            pos = 1;
        }
        if (pos > total + 1) {
            pos = total + 1;
        }
    }
    ev->id = next_id(g_head);
    ev->done = 0;
    now_str(ev->created, MAX_DUE);
    list_insert_at(&g_head, ev, pos);
    save_file(g_head, g_file);
    printf("已添加：P%d %s（原第 %d 位及之后的事件优先级已顺延）\n",
           ev->priority, ev->title, pos);
}

static Event *pick_by_id(const char *action)
{
    char buf[MAX_LINE];
    int id;
    Event *ev;
    printf("%s请输入编号（# 后面的数字）：", action);
    read_line(buf, sizeof(buf));
    id = atoi(buf);
    ev = list_find(g_head, id);
    if (ev == NULL) {
        printf("找不到编号为 %d 的记事。\n", id);
    }
    return ev;
}

static void do_modify(void)
{
    Event *ev = pick_by_id("修改");
    char buf[MAX_LINE];
    int total;
    int pos;
    if (ev == NULL) {
        return;
    }
    printf("当前：P%d [%s] %s\n", ev->priority, ev->done ? "x" : " ", ev->title);

    printf("新标题（回车保持不变）：");
    read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        sanitize(buf);
        safe_copy(ev->title, MAX_TITLE, buf);
    }
    printf("新截止时间（回车保持不变，输入 - 清除）：");
    read_line(buf, sizeof(buf));
    if (strcmp(buf, "-") == 0) {
        ev->due[0] = '\0';
    } else if (buf[0] != '\0') {
        if (valid_due(buf, 0)) {
            safe_copy(ev->due, MAX_DUE, buf);
        } else if (valid_due(buf, 1)) {
            sprintf(ev->due, "%s 00:00", buf);
        } else {
            printf("时间格式不对，保持原值。\n");
        }
    }
    printf("新备注（回车保持不变）：");
    read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        sanitize(buf);
        safe_copy(ev->note, MAX_NOTE, buf);
    }
    total = list_count(g_head);
    printf("新的优先级位置（1-%d，回车保持不变）：", total);
    read_line(buf, sizeof(buf));
    if (buf[0] != '\0') {
        pos = atoi(buf);
        if (pos >= 1 && pos <= total) {
            list_unlink(&g_head, ev);
            list_insert_at(&g_head, ev, pos);
            printf("优先级已调整。\n");
        } else {
            printf("位置超出范围，优先级保持不变。\n");
        }
    }
    save_file(g_head, g_file);
    printf("已保存修改。\n");
}

static void do_delete(void)
{
    Event *ev = pick_by_id("删除");
    char buf[8];
    if (ev == NULL) {
        return;
    }
    printf("确认删除「%s」？(y/N)：", ev->title);
    read_line(buf, sizeof(buf));
    if (buf[0] == 'y' || buf[0] == 'Y') {
        list_remove(&g_head, ev->id);
        save_file(g_head, g_file);
        printf("已删除，后续事件优先级已重排。\n");
    } else {
        printf("已取消。\n");
    }
}

static void do_toggle_done(void)
{
    Event *ev = pick_by_id("完成/取消");
    if (ev == NULL) {
        return;
    }
    ev->done = !ev->done;
    save_file(g_head, g_file);
    printf("%s：%s\n", ev->done ? "已标记完成" : "已恢复待办", ev->title);
}

static void do_reposition(void)
{
    Event *ev = pick_by_id("调整优先级");
    int total = list_count(g_head);
    int pos;
    if (ev == NULL) {
        return;
    }
    printf("当前优先级 P%d，共有 %d 条。新位置：", ev->priority, total);
    pos = read_int("");
    if (pos < 1) {
        pos = 1;
    }
    if (pos > total) {
        pos = total;
    }
    list_unlink(&g_head, ev);
    list_insert_at(&g_head, ev, pos);
    save_file(g_head, g_file);
    printf("「%s」现在是 P%d。\n", ev->title, pos);
}

static void do_query_date(void)
{
    char date[16];
    Event *cur;
    int n = 0;
    printf("\n查询日期（YYYY-MM-DD，回车=今天）：");
    read_line(date, sizeof(date));
    if (date[0] == '\0') {
        today_str(date, sizeof(date));
    } else if (!valid_due(date, 1)) {
        printf("日期格式不对。\n");
        return;
    }
    printf("\n--- %s 的安排 ---\n", date);
    for (cur = g_head; cur != NULL; cur = cur->next) {
        if (strncmp(cur->due, date, 10) == 0) {
            print_event(cur);
            n++;
        }
    }
    if (n == 0) {
        printf("  （当天没有安排）\n");
    }
}

static char *lower_dup(const char *s)
{
    size_t len = strlen(s);
    size_t i;
    char *out = (char *)malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    for (i = 0; i < len; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        out[i] = c;
    }
    out[len] = '\0';
    return out;
}

static void do_search(void)
{
    char keyword[MAX_LINE];
    char *key;
    Event *cur;
    int n = 0;
    printf("\n关键字：");
    read_line(keyword, sizeof(keyword));
    if (keyword[0] == '\0') {
        return;
    }
    key = lower_dup(keyword);
    if (key == NULL) {
        return;
    }
    printf("\n--- 搜索「%s」 ---\n", keyword);
    for (cur = g_head; cur != NULL; cur = cur->next) {
        char *t = lower_dup(cur->title);
        char *note = lower_dup(cur->note);
        int hit = (t != NULL && strstr(t, key) != NULL) ||
                  (note != NULL && strstr(note, key) != NULL);
        free(t);
        free(note);
        if (hit) {
            print_event(cur);
            n++;
        }
    }
    free(key);
    if (n == 0) {
        printf("  （没有匹配结果）\n");
    }
}

static void do_stats(void)
{
    int total = 0, done = 0, today = 0, overdue = 0;
    char today_buf[16];
    Event *cur;
    today_str(today_buf, sizeof(today_buf));
    for (cur = g_head; cur != NULL; cur = cur->next) {
        total++;
        if (cur->done) {
            done++;
        } else if (cur->due[0] != '\0') {
            if (strncmp(cur->due, today_buf, 10) == 0) {
                today++;
            } else if (strncmp(cur->due, today_buf, 10) < 0) {
                overdue++;
            }
        }
    }
    printf("\n统计：共 %d 条，已完成 %d 条，今天 %d 条，逾期 %d 条。\n",
           total, done, today, overdue);
    printf("数据文件：%s\n", g_file);
}

static void print_menu(void)
{
    printf("\n");
    printf("============== 墨记 · 电子记事本（C 版） ==============\n");
    printf("  1. 显示全部            2. 按日期查询     3. 搜索关键字\n");
    printf("  4. 新增记事（末尾）    5. 插入到指定位置 6. 修改记事\n");
    printf("  7. 标记完成/取消       8. 调整优先级     9. 删除记事\n");
    printf(" 10. 统计                11. 立即保存      0. 退出\n");
    printf("=======================================================\n");
    print_today_brief();
    printf("请选择：");
    fflush(stdout);
}

/* 菜单里的“今天概览”单独打印（不蜂鸣） */
static void print_today_brief(void)
{
    char today[16];
    Event *cur;
    int n = 0;
    today_str(today, sizeof(today));
    for (cur = g_head; cur != NULL; cur = cur->next) {
        if (!cur->done && strncmp(cur->due, today, 10) == 0) {
            n++;
        }
    }
    if (n > 0) {
        printf("今天有 %d 项安排\n", n);
    } else {
        printf("今天暂无安排\n");
    }
}

int main(int argc, char **argv)
{
    char buf[64];
    int choice;
    int loaded;

#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    if (argc > 1) {
        g_file = argv[1];
    }
    loaded = load_file(&g_head, g_file);
    printf("墨记 · 电子记事本（C 版）  数据文件：%s\n", g_file);
    printf("已载入 %d 条记事。\n", loaded);
    if (loaded > 0) {
        beep_reminder(g_head);
    }
    for (;;) {
        print_menu();
        read_line(buf, sizeof(buf));
        if (g_eof) {
            save_file(g_head, g_file);
            printf("\n输入结束，已自动保存并退出。\n");
            break;
        }
        choice = atoi(buf);
        switch (choice) {
        case 1:
            print_list(g_head, "全部记事");
            break;
        case 2:
            do_query_date();
            break;
        case 3:
            do_search();
            break;
        case 4:
            do_add(0);
            break;
        case 5:
            do_add(1);
            break;
        case 6:
            do_modify();
            break;
        case 7:
            do_toggle_done();
            break;
        case 8:
            do_reposition();
            break;
        case 9:
            do_delete();
            break;
        case 10:
            do_stats();
            break;
        case 11:
            if (save_file(g_head, g_file)) {
                printf("已保存到 %s。\n", g_file);
            }
            break;
        case 0:
            save_file(g_head, g_file);
            printf("已保存并退出。\n");
            list_clear(&g_head);
            return 0;
        default:
            printf("无效选项，请重新输入。\n");
            break;
        }
        if (g_eof) {
            save_file(g_head, g_file);
            printf("\n输入结束，已自动保存并退出。\n");
            break;
        }
    }
    list_clear(&g_head);
    return 0;
}
