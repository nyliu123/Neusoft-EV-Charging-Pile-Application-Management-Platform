QT += core network
QT -= gui

TEMPLATE = lib
CONFIG += staticlib
TARGET = ev_adapters

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src

HEADERS += map_api_adapter.h
SOURCES += map_api_adapter.cpp
HEADERS += consult_api_adapter.h
SOURCES += consult_api_adapter.cpp

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_common
PRE_TARGETDEPS += $$PLATFORM_BUILD_ROOT/lib/libev_common.a
