QT += core gui widgets network

TEMPLATE = app
TARGET = ev_user_client

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src
HEADERS += \
    $$PLATFORM_ROOT/src/client_ui/rhine_widgets.h \
    charge_flow_widget.h \
    order_list_widget.h \
    phone_login_widget.h \
    station_search_widget.h \
    user_api_client.h \
    user_home_widget.h \
    user_info_widget.h \
    user_session_state.h \
    $$PLATFORM_ROOT/src/client_ui/animated_combo_box.h \
    $$PLATFORM_ROOT/src/client_ui/client_style.h
SOURCES += \
    order_list_widget.cpp \
    main.cpp \
    charge_flow_widget.cpp \
    phone_login_widget.cpp \
    station_search_widget.cpp \
    user_api_client.cpp \
    user_home_widget.cpp \
    user_info_widget.cpp \
    user_session_state.cpp \
    $$PLATFORM_ROOT/src/client_ui/animated_combo_box.cpp \
    $$PLATFORM_ROOT/src/client_ui/client_style.cpp

RESOURCES += user_client.qrc

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_network -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_network.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a
