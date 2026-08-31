add_executable(evcs_admin_client
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(evcs_admin_client PRIVATE
    evcs_common
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
)
