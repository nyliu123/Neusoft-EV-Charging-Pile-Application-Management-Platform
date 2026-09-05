QT += core sql
QT -= gui

TEMPLATE = lib
CONFIG += staticlib
TARGET = ev_data

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src

HEADERS += database_manager.h
SOURCES += database_manager.cpp
RESOURCES += database.qrc

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_common
PRE_TARGETDEPS += $$PLATFORM_BUILD_ROOT/lib/libev_common.a

