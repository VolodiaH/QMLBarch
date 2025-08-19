cmake_minimum_required(VERSION 3.16)

project(qmlBarch VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Quick
    QuickControls2
    Concurrent
)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appqmlBarch
    main.cpp
)

qt_add_qml_module(appqmlBarch
    URI qmlBarch
    VERSION 1.0
    QML_FILES
        Main.qml
        SOURCES barch.cpp barch.hpp bmp_io.cpp bmp_io.h FileListModel.cpp FileListModel.h
        QML_FILES components/ErrorDialog.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appqmlBarch PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appqmlBarch
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appqmlBarch
    PRIVATE Qt6::Quick
    Qt6::Core
    Qt6::QuickControls2
    Qt6::Concurrent
)

include(GNUInstallDirs)
install(TARGETS appqmlBarch
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
