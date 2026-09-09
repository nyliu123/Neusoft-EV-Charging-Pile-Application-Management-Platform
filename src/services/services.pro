QT += core sql
QT -= gui

TEMPLATE = lib
CONFIG += staticlib
TARGET = ev_services

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src
HEADERS += membership_service.h sql_support.h
SOURCES += membership_service.cpp
HEADERS += knowledge_service.h
SOURCES += knowledge_service.cpp

HEADERS += \
    order_service.h \
    admin_auth_service.h \
    admin_seeder.h \
    charge_service.h \
    comment_service.h \
    fee_calculator.h \
    session_manager.h \
    station_service.h \
    user_service.h

SOURCES += \
    order_service.cpp \
    admin_auth_service.cpp \
    admin_seeder.cpp \
    charge_service.cpp \
    comment_service.cpp \
    fee_calculator.cpp \
    session_manager.cpp \
    station_service.cpp \
    user_service.cpp

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_data -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_data.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a
