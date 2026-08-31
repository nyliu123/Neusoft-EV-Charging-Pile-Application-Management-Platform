add_library(evcs_common STATIC
    apiclient.cpp
    apiclient.h
    domain.cpp
    domain.h
    protocol.cpp
    protocol.h
)

target_include_directories(evcs_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(evcs_common PUBLIC Qt6::Core Qt6::Network)
