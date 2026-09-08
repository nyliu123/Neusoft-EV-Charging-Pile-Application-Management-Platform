QT += core network sql testlib
QT -= gui
TEMPLATE = app
CONFIG += testcase
TARGET = ev_membership_tests
PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)
INCLUDEPATH += $$PLATFORM_ROOT/src
SOURCES += tst_membership.cpp
LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_services -lev_data -lev_common
PRE_TARGETDEPS += $$PLATFORM_BUILD_ROOT/lib/libev_services.a $$PLATFORM_BUILD_ROOT/lib/libev_data.a $$PLATFORM_BUILD_ROOT/lib/libev_common.a
