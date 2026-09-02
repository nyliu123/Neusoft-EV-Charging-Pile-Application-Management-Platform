QT += core network sql testlib
QT -= gui
CONFIG += console testcase c++17 warn_on
CONFIG -= app_bundle
TEMPLATE = app
TARGET = business_flow_test
INCLUDEPATH += $$PWD
DEFINES += EVCS_TEST_SCHEMA_PATH=\\\"$$PWD/schema.sql\\\"
SOURCES += \
    business_flow_test.cpp \
    businessservice.cpp \
    database.cpp \
    security.cpp \
    protocol.cpp \
    domain.cpp
HEADERS += businessservice.h database.h security.h protocol.h domain.h
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR = $$OUT_PWD/moc
