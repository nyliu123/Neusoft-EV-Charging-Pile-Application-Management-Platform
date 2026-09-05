QT += core
QT -= gui

TEMPLATE = lib
CONFIG += staticlib
TARGET = ev_network

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src

HEADERS += frame_codec.h
SOURCES += frame_codec.cpp

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_common
PRE_TARGETDEPS += $$PLATFORM_BUILD_ROOT/lib/libev_common.a

