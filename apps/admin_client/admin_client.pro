QT += core gui widgets network

TEMPLATE = app
TARGET = ev_admin_client

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src
INCLUDEPATH += $$PWD

HEADERS += \
    $$PLATFORM_ROOT/src/client_ui/apple_widgets.h \
    admin_add_station_dialog.h \
    admin_api_client.h \
    admin_charts.h \
    admin_dashboard_page.h \
    admin_format.h \
    admin_login_dialog.h \
    admin_main_window.h \
    admin_order_page.h \
    admin_pile_page.h \
    admin_session.h \
    admin_station_detail_page.h \
    admin_station_page.h \
    admin_user_page.h \
    $$PLATFORM_ROOT/src/client_ui/animated_combo_box.h \
    $$PLATFORM_ROOT/src/client_ui/client_style.h

SOURCES += \
    admin_add_station_dialog.cpp \
    admin_api_client.cpp \
    admin_charts.cpp \
    admin_dashboard_page.cpp \
    admin_login_dialog.cpp \
    admin_main_window.cpp \
    admin_order_page.cpp \
    admin_pile_page.cpp \
    admin_station_detail_page.cpp \
    admin_station_page.cpp \
    admin_user_page.cpp \
    main.cpp \
    $$PLATFORM_ROOT/src/client_ui/animated_combo_box.cpp \
    $$PLATFORM_ROOT/src/client_ui/client_style.cpp

RESOURCES += admin_client.qrc

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_network -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_network.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a
