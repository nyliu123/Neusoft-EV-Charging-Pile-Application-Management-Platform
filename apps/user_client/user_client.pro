QT += core gui widgets network

TEMPLATE = app
TARGET = ev_user_client

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src
HEADERS += \
    phone_login_widget.h \
    user_api_client.h \
    user_home_widget.h \
    user_info_widget.h \
    user_session_state.h
SOURCES += \
    main.cpp \
    phone_login_widget.cpp \
    user_api_client.cpp \
    user_home_widget.cpp \
    user_info_widget.cpp \
    user_session_state.cpp

RESOURCES += user_client.qrc

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_network -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_network.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a
