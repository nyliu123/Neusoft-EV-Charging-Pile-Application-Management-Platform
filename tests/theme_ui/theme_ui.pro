QT += core gui widgets network testlib webenginewidgets svg
TEMPLATE = app
CONFIG += testcase
TARGET = ev_theme_ui_tests
PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)
INCLUDEPATH += $$PLATFORM_ROOT/src $$PLATFORM_ROOT/apps/user_client $$PLATFORM_ROOT/apps/admin_client
SOURCES += tst_theme_ui.cpp \
    $$files($$PLATFORM_ROOT/apps/user_client/*.cpp) \
    $$files($$PLATFORM_ROOT/apps/admin_client/*.cpp) \
    $$PLATFORM_ROOT/src/client_ui/animated_combo_box.cpp \
    $$PLATFORM_ROOT/src/client_ui/client_style.cpp
SOURCES -= $$PLATFORM_ROOT/apps/user_client/main.cpp $$PLATFORM_ROOT/apps/admin_client/main.cpp
HEADERS += $$files($$PLATFORM_ROOT/apps/user_client/*.h) \
    $$files($$PLATFORM_ROOT/apps/admin_client/*.h) \
    $$files($$PLATFORM_ROOT/src/client_ui/*.h)
RESOURCES += $$PLATFORM_ROOT/apps/admin_client/admin_client.qrc \
    $$PLATFORM_ROOT/apps/user_client/user_client.qrc
LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_network -lev_common
PRE_TARGETDEPS += $$PLATFORM_BUILD_ROOT/lib/libev_network.a $$PLATFORM_BUILD_ROOT/lib/libev_common.a
