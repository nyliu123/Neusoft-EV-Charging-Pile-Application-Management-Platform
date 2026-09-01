find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(protocol_test protocol_test.cpp)
target_link_libraries(protocol_test PRIVATE evcs_common Qt6::Core Qt6::Test)
add_test(NAME protocol_test COMMAND protocol_test)

add_executable(business_flow_test business_flow_test.cpp)
target_link_libraries(business_flow_test PRIVATE
    evcs_server_core
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)
target_compile_definitions(business_flow_test PRIVATE
    EVCS_TEST_SCHEMA_PATH="${CMAKE_SOURCE_DIR}/database/schema.sql"
)
add_test(NAME business_flow_test COMMAND business_flow_test)

add_executable(socket_integration_test
    socket_integration_test.cpp
    ../src/server/chargingserver.cpp
    ../src/server/chargingserver.h
)
target_link_libraries(socket_integration_test PRIVATE
    evcs_server_core
    Qt6::Concurrent
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
target_compile_definitions(socket_integration_test PRIVATE
    EVCS_TEST_SCHEMA_PATH="${PROJECT_SOURCE_DIR}/database/schema.sql"
)
add_test(NAME socket_integration_test COMMAND socket_integration_test)

add_executable(ui_acceptance_test
    ui_acceptance_test.cpp
    ../src/server/chargingserver.cpp
    ../src/server/chargingserver.h
    ../src/user_client/mainwindow.cpp
    ../src/user_client/mainwindow.h
    ../src/admin_client/mainwindow.cpp
    ../src/admin_client/mainwindow.h
)
target_link_libraries(ui_acceptance_test PRIVATE
    evcs_server_core
    Qt6::Concurrent
    Qt6::Core
    Qt6::Network
    Qt6::Test
    Qt6::Widgets
    Qt6::Charts
    Qt6::WebEngineWidgets
)
target_compile_definitions(ui_acceptance_test PRIVATE
    EVCS_TEST_SCHEMA_PATH="${PROJECT_SOURCE_DIR}/database/schema.sql"
)
add_test(NAME ui_acceptance_test COMMAND ui_acceptance_test)
set_tests_properties(ui_acceptance_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EVCS_TEST_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/test-artifacts"
)

add_test(NAME preprocessing_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/preprocessing_test.py
            ${PROJECT_SOURCE_DIR}/tools/preprocess_analytics.py
)
