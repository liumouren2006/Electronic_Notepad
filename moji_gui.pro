QT += widgets
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
    src/core/util.cpp \
    src/core/model.cpp \
    src/core/elist.cpp \
    src/core/store.cpp \
    src/core/zip.cpp \
    src/core/repository.cpp \
    src/core/importer.cpp

HEADERS += \
    src/gui/qt_gui.hpp \
    src/gui/gui.hpp \
    src/core/util.hpp \
    src/core/model.hpp \
    src/core/elist.hpp \
    src/core/store.hpp \
    src/core/repository.hpp \
    src/core/importer.hpp

RESOURCES += src/assets/assets.qrc

LIBS += -lshell32 -lole32
