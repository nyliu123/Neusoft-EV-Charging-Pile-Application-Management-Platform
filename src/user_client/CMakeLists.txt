add_executable(evcs_user_client
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(evcs_user_client PRIVATE
    evcs_common
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
    Qt6::WebEngineWidgets
)
