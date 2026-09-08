QT += core gui widgets network

TEMPLATE = app
TARGET = ev_admin_client

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src
SOURCES += main.cpp \
    addstationdialog.cpp \
    screendataservice.cpp \
    stationdetaildialog.cpp

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_network -lev_common
PRE_TARGETDEPS += \
    $$PLATFORM_BUILD_ROOT/lib/libev_network.a \
    $$PLATFORM_BUILD_ROOT/lib/libev_common.a

FORMS += \
    addstationdialog.ui

HEADERS += \
    addstationdialog.h \
    screendataservice.h \
    stationdetaildialog.h

