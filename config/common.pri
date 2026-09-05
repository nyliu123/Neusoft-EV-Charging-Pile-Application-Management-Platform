CONFIG += c++17 warn_on

QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic

isEmpty(PLATFORM_ROOT) {
    error("PLATFORM_ROOT must point to the repository root")
}

PLATFORM_BUILD_ROOT = $$clean_path($$OUT_PWD/../..)
contains(TEMPLATE, app) {
    DESTDIR = $$PLATFORM_BUILD_ROOT/bin
} else {
    DESTDIR = $$PLATFORM_BUILD_ROOT/lib
}

OBJECTS_DIR = $$PLATFORM_BUILD_ROOT/obj/$$TARGET
MOC_DIR = $$PLATFORM_BUILD_ROOT/moc/$$TARGET
RCC_DIR = $$PLATFORM_BUILD_ROOT/rcc/$$TARGET
UI_DIR = $$PLATFORM_BUILD_ROOT/ui/$$TARGET
