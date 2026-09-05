QT += core gui widgets network

TEMPLATE = app
TARGET = ev_admin_client

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src
INCLUDEPATH += $$PWD

HEADERS += \
    admin_api_client.h \
    admin_charts.h \
    admin_dashboard_page.h \
    admin_format.h \
    admin_login_dialog.h \
    admin_main_window.h \
    admin_pile_page.h \
    admin_session.h

SOURCES += \
    admin_api_client.cpp \
    admin_charts.cpp \
    admin_dashboard_page.cpp \
    admin_login_dialog.cpp \
    admin_main_window.cpp \
    admin_pile_page.cpp \
    main.cpp

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_network -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_network.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a
