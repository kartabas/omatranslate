QT += core gui quick quickcontrols2 testlib dbus network

CONFIG += c++17 testcase
TARGET = omatranslate_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/backend.h \
    ../src/filepicker.h \
    ../src/googletranslate.h \
    ../src/languages.h \
    ../src/selectionwatcher.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    omatranslate_tests.cpp \
    ../src/backend.cpp \
    ../src/googletranslate.cpp \
    ../src/languages.cpp \
    ../src/selectionwatcher.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp
