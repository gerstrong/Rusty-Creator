cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

project(Rusty)

add_subdirectory(plugins)

set(PROJECT_WIZARD_PATH "$ENV{HOME}/.config/QtProject/qtcreator/templates/wizards/projects/rustapplication")

file(MAKE_DIRECTORY ${PROJECT_WIZARD_PATH})

Message(STATUS "Wizards templates will be copied after building Rusty into ${PROJECT_WIZARD_PATH}")

# Copy the wizard template every time the plugin gets built
add_custom_target(
    TARGET ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/templates/wizards/projects/rustapplication
            ${PROJECT_WIZARD_PATH}
)
