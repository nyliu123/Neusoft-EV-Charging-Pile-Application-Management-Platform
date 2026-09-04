QT += core network sql concurrent
QT -= gui
CONFIG += console c++17 warn_on
CONFIG -= app_bundle
TEMPLATE = app
TARGET = evcs_server
INCLUDEPATH += $$PWD

SOURCES += \
    server_main.cpp \
    server_settings.cpp \
    chargingserver.cpp \
    mapapiadapter.cpp \
    businessservice.cpp \
    database.cpp \
    security.cpp \
    logging.cpp \
    protocol.cpp \
    domain.cpp

HEADERS += \
    server_settings.h \
    chargingserver.h \
    mapapiadapter.h \
    businessservice.h \
    database.h \
    security.h \
    logging.h \
    protocol.h \
    domain.h

OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR = $$OUT_PWD/moc
RCC_DIR = $$OUT_PWD/rcc
UI_DIR = $$OUT_PWD/ui
