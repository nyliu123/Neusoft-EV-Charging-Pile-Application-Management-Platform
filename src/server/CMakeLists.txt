add_library(evcs_server_core STATIC
    businessservice.cpp
    businessservice.h
    database.cpp
    database.h
    security.cpp
    security.h
)

target_link_libraries(evcs_server_core PUBLIC
    evcs_common
    Qt6::Core
    Qt6::Sql
)

target_include_directories(evcs_server_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(evcs_server
    chargingserver.cpp
    chargingserver.h
    logging.cpp
    logging.h
    main.cpp
)

target_link_libraries(evcs_server PRIVATE
    evcs_server_core
    Qt6::Core
    Qt6::Network
)
