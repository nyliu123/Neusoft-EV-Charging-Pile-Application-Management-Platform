QT += core gui widgets network webenginewidgets svg testlib
TEMPLATE = app
CONFIG += testcase
TARGET = ev_order_ui_tests
PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)
INCLUDEPATH += $$PLATFORM_ROOT/src $$PLATFORM_ROOT/apps/user_client
HEADERS += $$PLATFORM_ROOT/apps/user_client/membership_dialog.h
SOURCES += $$PLATFORM_ROOT/apps/user_client/membership_dialog.cpp
SOURCES += tst_order_ui.cpp \
    $$PLATFORM_ROOT/apps/user_client/order_list_widget.cpp \
    $$PLATFORM_ROOT/apps/user_client/user_api_client.cpp \
    $$PLATFORM_ROOT/apps/user_client/user_session_state.cpp
HEADERS += $$PLATFORM_ROOT/apps/user_client/order_list_widget.h \
    $$PLATFORM_ROOT/apps/user_client/user_api_client.h
LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_network -lev_common
PRE_TARGETDEPS += $$PLATFORM_BUILD_ROOT/lib/libev_network.a $$PLATFORM_BUILD_ROOT/lib/libev_common.a

SOURCES += $$PLATFORM_ROOT/apps/user_client/user_home_widget.cpp \
    $$PLATFORM_ROOT/apps/user_client/charge_flow_widget.cpp \
    $$PLATFORM_ROOT/apps/user_client/station_search_widget.cpp \
    $$PLATFORM_ROOT/apps/user_client/user_info_widget.cpp \
    $$PLATFORM_ROOT/src/client_ui/animated_combo_box.cpp
HEADERS += $$PLATFORM_ROOT/apps/user_client/user_home_widget.h \
    $$PLATFORM_ROOT/apps/user_client/charge_flow_widget.h \
    $$PLATFORM_ROOT/apps/user_client/station_search_widget.h \
    $$PLATFORM_ROOT/apps/user_client/user_info_widget.h \
    $$PLATFORM_ROOT/src/client_ui/animated_combo_box.h
RESOURCES += $$PLATFORM_ROOT/apps/user_client/user_client.qrc

SOURCES += $$PLATFORM_ROOT/apps/user_client/navigation_map_dialog.cpp
HEADERS += $$PLATFORM_ROOT/apps/user_client/navigation_map_dialog.h
SOURCES += $$PLATFORM_ROOT/src/client_ui/client_style.cpp
HEADERS += $$PLATFORM_ROOT/src/client_ui/client_style.h $$PLATFORM_ROOT/src/client_ui/apple_widgets.h
