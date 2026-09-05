QT += core
QT -= gui

TEMPLATE = lib
CONFIG += staticlib
TARGET = ev_common

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src

HEADERS += \
    error_code.h \
    protocol.h \
    result.h

SOURCES += common.cpp

