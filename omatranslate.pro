QT += core gui qml quick quickcontrols2 dbus network

CONFIG += c++17 release
TARGET = omatranslate
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/googletranslate.h \
    src/languages.h \
    src/overlay.h \
    src/placement.h \
    src/portalfilepicker.h \
    src/selectionwatcher.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/backend.cpp \
    src/cli.cpp \
    src/googletranslate.cpp \
    src/languages.cpp \
    src/main.cpp \
    src/overlay.cpp \
    src/placement.cpp \
    src/portalfilepicker.cpp \
    src/selectionwatcher.cpp \
    src/theme.cpp \
    src/translator.cpp

INCLUDEPATH += /usr/include/LayerShellQt
LIBS += -lLayerShellQtInterface

RESOURCES += src/resources.qrc
