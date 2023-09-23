cmake_minimum_required(VERSION 3.10)

## TODO: Remove when sharing with others.
set(QT_CREATOR_BUILD_PATH "/home/gerhard/workspace/Rusty-Creator/qt-creator/build")
list(APPEND CMAKE_PREFIX_PATH "${QT_CREATOR_BUILD_PATH}")
#
set(CMAKE_CXX_STANDARD 17)

project(Rusty)
#
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTORCC ON)
#set(CMAKE_AUTOUIC ON)

#
#find_package(QtCreator REQUIRED COMPONENTS Core)
#find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
#set(QtX Qt${QT_VERSION_MAJOR})
#
#
## Add a CMake option that enables building your plugin with tests.
## You don't want your released plugin binaries to contain tests,
## so make that default to 'NO'.
## Enable tests by passing -DWITH_TESTS=ON to CMake.
#option(WITH_TESTS "Builds with tests" NO)
#
#if(WITH_TESTS)
#  # Look for QtTest
#  find_package(${QtX} REQUIRED COMPONENTS Test)
##
#  # Tell CMake functions like add_qtc_plugin about the QtTest component.
#  set(IMPLICIT_DEPENDS Qt::Test)
#
#  # Enable ctest for auto tests.
#  enable_testing()
#endif()

add_subdirectory(plugins)

#set(PROJECT_WIZARD_PATH "${QT_CREATOR_BUILD_PATH}/share/qtcreator/templates/wizards/projects/rustapplication")
set(PROJECT_WIZARD_PATH "$ENV{HOME}/.config/QtProject/qtcreator/templates/wizards/projects/rustapplication")

file(MAKE_DIRECTORY ${PROJECT_WIZARD_PATH})

## Or we have to copy it into ${HOME}/.config/QtProject/qtcreator/

Message(STATUS "Wizards templates will be copied after building Rusty into ${PROJECT_WIZARD_PATH}")

# Copy the wizard template every time the plugin gets built
add_custom_target(
    TARGET ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/templates/wizards/projects/rustapplication
            ${PROJECT_WIZARD_PATH}
)
