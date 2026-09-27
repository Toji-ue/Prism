# prism_canvas — compositing + brush. CPU reference implementation; the
# Qt RHI renderer lands here later and must match this output exactly.

add_library(prism_canvas STATIC
    compositor/Compositor.cpp
    compositor/Compositor.h
    brush/BrushStroke.cpp
    brush/BrushStroke.h)

add_library(Prism::canvas ALIAS prism_canvas)

target_link_libraries(prism_canvas PUBLIC Prism::core)
target_include_directories(prism_canvas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
    target_compile_options(prism_canvas PRIVATE /W4)
else()
    target_compile_options(prism_canvas PRIVATE -Wall -Wextra -Wpedantic)
endif()
