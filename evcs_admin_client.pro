QT += core network widgets charts
CONFIG += c++17 warn_on
CONFIG -= app_bundle
TEMPLATE = app
TARGET = evcs_admin_client
INCLUDEPATH += $$PWD

SOURCES += \
    admin_main.cpp \
    admin_mainwindow.cpp \
    admin_pages.cpp \
    admin_actions.cpp \
    admin_responses.cpp \
    admin_style.cpp \
    style_loader.cpp \
    ui_text.cpp \
    apiclient.cpp \
    protocol.cpp \
    domain.cpp

HEADERS += \
    admin_mainwindow.h \
    admin_style.h \
    style_loader.h \
    ui_text.h \
    apiclient.h \
    protocol.h \
    domain.h

RESOURCES += admin_resources.qrc
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR = $$OUT_PWD/moc
RCC_DIR = $$OUT_PWD/rcc
UI_DIR = $$OUT_PWD/ui
