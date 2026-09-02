QT += core network sql concurrent widgets charts webenginewidgets testlib
CONFIG += console testcase c++17 warn_on
CONFIG -= app_bundle
TEMPLATE = app
TARGET = ui_acceptance_test
INCLUDEPATH += $$PWD
DEFINES += EVCS_TEST_SCHEMA_PATH=\\\"$$PWD/schema.sql\\\"
SOURCES += \
    ui_acceptance_test.cpp \
    chargingserver.cpp \
    businessservice.cpp \
    database.cpp \
    security.cpp \
    protocol.cpp \
    domain.cpp \
    apiclient.cpp \
    style_loader.cpp \
    ui_text.cpp \
    user_style.cpp \
    user_mainwindow.cpp \
    user_pages.cpp \
    user_actions.cpp \
    user_responses.cpp \
    admin_style.cpp \
    admin_mainwindow.cpp \
    admin_pages.cpp \
    admin_actions.cpp \
    admin_responses.cpp
HEADERS += \
    chargingserver.h businessservice.h database.h security.h protocol.h domain.h apiclient.h \
    style_loader.h ui_text.h user_style.h user_mainwindow.h admin_style.h admin_mainwindow.h
RESOURCES += user_resources.qrc admin_resources.qrc
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR = $$OUT_PWD/moc
RCC_DIR = $$OUT_PWD/rcc
