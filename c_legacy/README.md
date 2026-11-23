# C 语言版 · 电子记事本

`notebook.c` 是课程设计基础要求的独立 C89 实现，可直接用 VC++6.0 / VS / GCC 编译。

## 功能

- 单向链表按优先级保存事件，**插入到第 N 位后，原 N 及之后的事件自动 +1**
- UTF-8 文本文件持久化（`notes.dat`，管道分隔）
- 菜单 + 循环：一次运行可连续做多项管理
- 显示全部、按日期查询、关键字搜索
- 新增、插入到指定位置、修改、调整优先级、标记完成、删除、统计
- 启动时若当天有安排，调用 Windows `Beep` 蜂鸣提醒
- 输入校验、长度限制、坏行跳过、临时文件 + 原子替换保存

## 编译

GCC / MinGW：

```bash
gcc -std=c89 -Wall -Wextra -O2 -o notebook notebook.c
```

MSYS2 如果出现“静默失败”，先把 `ucrt64/bin` 加入 PATH：

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"
```

Windows 双击构建：`build.bat`。Linux/macOS：`bash build.sh`。

VC++6.0：

1. 新建 Win32 Console Application，加入 `notebook.c`；
2. 源文件里的中文是 UTF-8，VC6 默认按 GBK 解析，请用记事本/VS 将源文件另存为 **ANSI/GBK**；
3. 若不需要彩色/UTF-8 控制台，可删除 `main` 中的 `SetConsoleOutputCP` 两行；
4. 编译运行即可。`Beep` 需要包含 `<windows.h>`（已用 `_WIN32` 条件编译）。

## 运行

```bash
./notebook                 # 使用当前目录 notes.dat
./notebook D:\data\my.dat  # 指定数据文件
```

首次运行会自动创建数据文件。菜单：

```
 1. 显示全部            2. 按日期查询     3. 搜索关键字
 4. 新增记事（末尾）    5. 插入到指定位置 6. 修改记事
 7. 标记完成/取消       8. 调整优先级     9. 删除记事
10. 统计                11. 立即保存      0. 退出
```

## 数据格式

```
# MOJI-C 1
1|1|2026-11-24 18:00|0|买牛奶|超市采购
2|2|2026-11-25 09:00|1|写报告|课程设计
```

字段：`id|优先级|截止时间|是否完成|标题|备注`。输入中的 `|` 和换行会被替换为空格，保证一行一条记录。

## 与 Python 版的关系

- C 版覆盖**基础要求**，不包含附件、GUI、TUI、导入导出；
- Python 版覆盖基础要求与全部拓展；
- 两版的优先级规则、文本存储思想一致，可对照答辩。

## 手工回归

```bash
printf '4\n买牛奶\n2026-11-24 18:00\n超市\n5\n插队\n2026-11-24 12:00\n\n2\n1\n0\n' \
  | ./notebook /tmp/notes.dat
```

预期：插入“插队”后，原 P2 事件变为 P3；`notes.dat` 中前三条优先级为 1、2、3。
