QT += core network sql
QT -= gui

TEMPLATE = app
TARGET = ev_server

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

# Absolute apps/server source directory, baked in at qmake time. main.cpp uses
# it to locate the shared runtime database: qmake compiles with source paths
# relative to the per-project build directory, and resolving them against the
# launch working directory (bin/) used to miss the source tree and silently
# fall back to a private per-build-dir database.
DEFINES += EV_SERVER_SOURCE_DIR=\\\"$$PWD\\\"

INCLUDEPATH += $$PLATFORM_ROOT/src

HEADERS += \
    server_application.h \
    admin_handler.h \
    charging_session_manager.h
SOURCES += \
    main.cpp \
    server_application.cpp \
    admin_handler.cpp \
    charging_session_manager.cpp

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
