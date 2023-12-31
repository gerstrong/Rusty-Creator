cmake_minimum_required(VERSION 3.10)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
set(QtX Qt${QT_VERSION_MAJOR})

# Add a CMake option that enables building your plugin with tests.
# You don't want your released plugin binaries to contain tests,
# so make that default to 'NO'.
# Enable tests by passing -DWITH_TESTS=ON to CMake.
option(WITH_TESTS "Builds with tests" NO)

if(WITH_TESTS)
  # Look for QtTest
  find_package(${QtX} REQUIRED COMPONENTS Test)

  # Tell CMake functions like add_qtc_plugin about the QtTest component.
  set(IMPLICIT_DEPENDS Qt::Test)

  # Enable ctest for auto tests.
  enable_testing()
endif()

add_qtc_plugin(Rusty
  PLUGIN_DEPENDS
    QtCreator::Core
  DEPENDS
    ${QtX}::Widgets
    QtCreator::ExtensionSystem
    QtCreator::Utils
    QtCreator::LanguageClient
    QtCreator::ProjectExplorer
    QtCreator::TextEditor
    QtCreator::QmlJSEditor
    QtCreator::QmlJSTools
  SOURCES
    ../../README.md
    rusty.cpp
    rusty.h
    rusty_global.h
    rustyconstants.h
)


target_sources(Rusty
  PRIVATE
    rusteditor.h rusteditor.cpp
    rustutils.h
    rustsettings.h
    rusttr.h
    rustlanguageclient.h rustlanguageclient.cpp
    rsside.h
    rustsettings.cpp
    rustproject.h rustproject.cpp
    rustwizardpagefactory.h rustwizardpagefactory.cpp
    rustrunconfiguration.h rustrunconfiguration.cpp
    cratesupport.h cratesupport.cpp
    rssideuicextracompiler.h rssideuicextracompiler.cpp
    rssidebuildconfiguration.h rssidebuildconfiguration.cpp
    rusthighlighter.h rusthighlighter.cpp

    rustscanner.h rustscanner.cpp
    rustformattoken.h
    rustindenter.h rustindenter.cpp
    rustutils.cpp
    rsside.cpp
)


