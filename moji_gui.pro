QT += widgets network
CONFIG += c++17 release
win32: CONFIG += windows
CONFIG -= debug_and_release
TEMPLATE = app
TARGET = moji-gui

DESTDIR = $$PWD/build
OBJECTS_DIR = $$PWD/build/obj-gui
MOC_DIR = $$PWD/build/moc-gui
RCC_DIR = $$PWD/build/rcc-gui

DEFINES += UNICODE _UNICODE
INCLUDEPATH += $$PWD/src $$PWD/third_party

SOURCES += \
    src/gui_main.cpp \
    src/gui/qt_gui.cpp \
    src/gui/markdown.cpp \
    src/gui/cloud_client.cpp \
    src/gui/page_view.cpp \
    src/core/util.cpp \
    src/core/model.cpp \
    src/core/elist.cpp \
    src/core/store.cpp \
    src/core/zip.cpp \
    src/core/repository.cpp \
    src/core/importer.cpp \
    src/core/sound.cpp

HEADERS += \
    src/gui/qt_gui.hpp \
    src/gui/gui.hpp \
    src/gui/markdown.hpp \
    src/gui/cloud_client.hpp \
    src/gui/page_view.hpp \
    src/core/util.hpp \
    src/core/model.hpp \
    src/core/elist.hpp \
    src/core/store.hpp \
    src/core/repository.hpp \
    src/core/importer.hpp \
    src/core/sound.hpp

RESOURCES += src/assets/assets.qrc

LIBS += -lshell32 -lole32 -lwinmm

# windres 无法在中文路径上写绝对输出文件；在构建目录生成资源对象再用相对路径链接。
win32 {
    QMAKE_PRE_LINK += windres -i $$relative_path($$PWD/src/app.rc, $$OUT_PWD) -o app_res.o
    LIBS += app_res.o
}
