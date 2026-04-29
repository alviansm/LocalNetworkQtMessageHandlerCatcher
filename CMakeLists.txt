cmake_minimum_required(VERSION 3.19)
project(LocalNetworkQtMessageHandlerCatcher LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network LinguistTools)

qt_standard_project_setup()

qt_add_executable(LocalNetworkQtMessageHandlerCatcher
    WIN32 MACOSX_BUNDLE
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
)

qt_add_translations(
    TARGETS LocalNetworkQtMessageHandlerCatcher
    TS_FILES LocalNetworkQtMessageHandlerCatcher_en_001.ts
)

target_link_libraries(LocalNetworkQtMessageHandlerCatcher
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)

include(GNUInstallDirs)

install(TARGETS LocalNetworkQtMessageHandlerCatcher
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET LocalNetworkQtMessageHandlerCatcher
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
