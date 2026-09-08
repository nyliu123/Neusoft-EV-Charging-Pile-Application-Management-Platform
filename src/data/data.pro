QT += core sql
QT -= gui

TEMPLATE = lib
CONFIG += staticlib
TARGET = ev_data

PLATFORM_ROOT = $$clean_path($$PWD/../..)
include($$PLATFORM_ROOT/config/common.pri)

INCLUDEPATH += $$PLATFORM_ROOT/src

HEADERS += \
    database_manager.h \
    station_repository.h \
    user_repository.h \
    pile_repository.h \
    order_repository.h \
    admin_repository.h \
    comment_repository.h
SOURCES += \
    database_manager.cpp \
    station_repository.cpp \
    user_repository.cpp \
    pile_repository.cpp \
    order_repository.cpp \
    admin_repository.cpp \
    comment_repository.cpp
RESOURCES += database.qrc

LIBS += -L$$PLATFORM_BUILD_ROOT/lib -lev_common
PRE_TARGETDEPS += $$PLATFORM_BUILD_ROOT/lib/libev_common.a
