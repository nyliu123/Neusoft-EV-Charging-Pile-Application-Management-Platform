QT += core network widgets webenginewidgets
CONFIG += c++17 warn_on
CONFIG -= app_bundle
TEMPLATE = app
TARGET = evcs_user_client
INCLUDEPATH += $$PWD

SOURCES += \
    user_main.cpp \
    user_mainwindow.cpp \
    user_pages.cpp \
    user_actions.cpp \
    user_responses.cpp \
    user_session.cpp \
    station_card.cpp \
    user_style.cpp \
    style_loader.cpp \
    ui_text.cpp \
    apiclient.cpp \
    protocol.cpp \
    domain.cpp

HEADERS += \
    user_mainwindow.h \
    user_session.h \
    station_card.h \
    user_style.h \
    style_loader.h \
    ui_text.h \
    apiclient.h \
    protocol.h \
    domain.h

RESOURCES += user_resources.qrc
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR = $$OUT_PWD/moc
RCC_DIR = $$OUT_PWD/rcc
UI_DIR = $$OUT_PWD/ui
