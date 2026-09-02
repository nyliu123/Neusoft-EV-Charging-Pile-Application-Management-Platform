QT += core network sql concurrent testlib
QT -= gui
CONFIG += console testcase c++17 warn_on
CONFIG -= app_bundle
TEMPLATE = app
TARGET = socket_integration_test
INCLUDEPATH += $$PWD
DEFINES += EVCS_TEST_SCHEMA_PATH=\\\"$$PWD/schema.sql\\\"
SOURCES += \
    socket_integration_test.cpp \
    apiclient.cpp \
    chargingserver.cpp \
    businessservice.cpp \
    database.cpp \
    security.cpp \
    protocol.cpp \
    domain.cpp
HEADERS += apiclient.h chargingserver.h businessservice.h database.h security.h protocol.h domain.h
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR = $$OUT_PWD/moc
