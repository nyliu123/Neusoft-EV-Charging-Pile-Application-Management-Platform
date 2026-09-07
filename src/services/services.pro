QT += core sql
QT -= gui

TEMPLATE = lib
CONFIG += staticlib
TARGET = ev_services

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src

HEADERS += \
    admin_auth_service.h \
    admin_seeder.h \
    fee_calculator.h \
    station_service.h \
    session_manager.h \
    user_service.h

SOURCES += \
    admin_auth_service.cpp \
    admin_seeder.cpp \
    fee_calculator.cpp \
    station_service.cpp \
    session_manager.cpp \
    user_service.cpp

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_data -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_data.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a
