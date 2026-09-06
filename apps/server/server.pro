QT += core network sql
QT -= gui

TEMPLATE = app
TARGET = ev_server

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src

HEADERS += server_application.h admin_handler.h
SOURCES += main.cpp server_application.cpp admin_handler.cpp

LIBS += -L$$PLATFORM_BUILD_ROOT/lib \
    -lev_adapters \
    -lev_services \
    -lev_data \
    -lev_network \
    -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_adapters.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_services.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_data.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_network.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a
