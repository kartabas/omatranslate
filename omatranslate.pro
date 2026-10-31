QT += core gui qml quick quickcontrols2 dbus network

CONFIG += c++17 release
TARGET = omatranslate
TEMPLATE = app

HEADERS += \
    src/appcontroller.h \
    src/backend.h \
    src/cli.h \
    src/controlserver.h \
    src/deepltranslate.h \
    src/filepicker.h \
    src/googletranslate.h \
    src/languages.h \
    src/libretranslate.h \
    src/overlay.h \
    src/placement.h \
    src/portalfilepicker.h \
    src/selectionwatcher.h \
    src/theme.h \
    src/translationprovider.h \
    src/translationservice.h \
    src/translator.h

SOURCES += \
    src/appcontroller.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/controlserver.cpp \
    src/deepltranslate.cpp \
    src/googletranslate.cpp \
    src/languages.cpp \
    src/libretranslate.cpp \
    src/main.cpp \
    src/overlay.cpp \
    src/placement.cpp \
    src/portalfilepicker.cpp \
    src/selectionwatcher.cpp \
    src/theme.cpp \
    src/translationprovider.cpp \
    src/translationservice.cpp \
    src/translator.cpp

INCLUDEPATH += /usr/include/LayerShellQt
LIBS += -lLayerShellQtInterface

RESOURCES += src/resources.qrc
