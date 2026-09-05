QT += core gui widgets network

TEMPLATE = app
TARGET = ev_admin_client

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src
INCLUDEPATH += $$PWD

HEADERS += \
    admin_login_dialog.h \
    admin_main_window.h \
    admin_session.h

SOURCES += \
    admin_login_dialog.cpp \
    admin_main_window.cpp \
    main.cpp

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_network -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_network.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a
