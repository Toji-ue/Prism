# prism_io — file format read/write. Qt allowed (QImage for codecs).

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_library(prism_io STATIC
    project/ProjectIO.cpp
    project/ProjectIO.h)

add_library(Prism::io ALIAS prism_io)

target_link_libraries(prism_io PUBLIC Prism::core Prism::canvas Qt6::Core Qt6::Gui)
target_include_directories(prism_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
