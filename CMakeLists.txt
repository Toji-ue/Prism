cmake_minimum_required(VERSION 3.24)

project(Prism
    VERSION 0.1.0
    DESCRIPTION "Prism — native desktop drawing & illustration app"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt: auto moc/qml registration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick)
qt_standard_project_setup()

# --------------------------------------------------------------------------- #
# prism_core — framework-agnostic domain logic (no Qt/GPU types allowed)
add_subdirectory(src/core)

# --------------------------------------------------------------------------- #
# prism app
qt_add_executable(prism
    src/app/main.cpp)

qt_add_qml_module(prism
    URI Prism
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml)

target_link_libraries(prism PRIVATE
    Prism::core
    Qt6::Quick)

# --------------------------------------------------------------------------- #
# tests
enable_testing()
add_subdirectory(tests)
