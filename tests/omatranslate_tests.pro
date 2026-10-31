QT += core gui quick quickcontrols2 testlib dbus network

CONFIG += c++17 testcase
TARGET = omatranslate_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/backend.h \
    ../src/deepltranslate.h \
    ../src/filepicker.h \
    ../src/googletranslate.h \
    ../src/languages.h \
    ../src/libretranslate.h \
    ../src/placement.h \
    ../src/selectionwatcher.h \
    ../src/theme.h \
    ../src/translationprovider.h \
    ../src/translationservice.h \
    ../src/translator.h

SOURCES += \
    omatranslate_tests.cpp \
    ../src/backend.cpp \
    ../src/deepltranslate.cpp \
    ../src/googletranslate.cpp \
    ../src/languages.cpp \
    ../src/libretranslate.cpp \
    ../src/placement.cpp \
    ../src/selectionwatcher.cpp \
    ../src/theme.cpp \
    ../src/translationprovider.cpp \
    ../src/translationservice.cpp \
    ../src/translator.cpp
