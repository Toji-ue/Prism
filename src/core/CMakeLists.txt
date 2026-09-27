# prism_core — domain logic. No Qt / GPU types allowed in this target.

add_library(prism_core STATIC
    commands/Commands.cpp
    commands/Commands.h
    document/Document.cpp
    document/Document.h
    layer/Layer.cpp
    layer/Layer.h
    layer/BlendMode.h
    history/Command.h
    history/History.cpp
    history/History.h
    geometry/Types.h
    raster/RasterBuffer.cpp
    raster/RasterBuffer.h
    raster/Blend.cpp
    raster/Blend.h
    raster/ColorMath.cpp
    raster/ColorMath.h)

add_library(Prism::core ALIAS prism_core)

target_include_directories(prism_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
    target_compile_options(prism_core PRIVATE /W4)
else()
    target_compile_options(prism_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
