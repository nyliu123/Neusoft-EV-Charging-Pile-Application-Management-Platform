QT += core sql
QT -= gui

TEMPLATE = lib
CONFIG += staticlib
TARGET = ev_services

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src

HEADERS += \
    fee_calculator.h \
    session_manager.h \
    user_service.h
SOURCES += \
    fee_calculator.cpp \
    session_manager.cpp \
    user_service.cpp

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_data -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_data.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a
