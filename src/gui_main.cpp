// Qt 图形界面入口（窗口子系统，经 qmake CONFIG += windows + qtmain 调用 main）
#include "gui/qt_gui.hpp"

int main(int argc, char** argv) { return moji::run_qt_gui(argc, argv); }
