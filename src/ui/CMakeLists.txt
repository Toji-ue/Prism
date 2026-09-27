# prism_ui — QML-facing C++ bridge (controller, canvas view, layer model).
# Qt allowed here, but domain logic stays in Prism::core / Prism::canvas.
# Types are registered with qmlRegisterType in main.cpp.

add_library(prism_ui STATIC
    application/AppController.cpp
    application/AppController.h
    application/LayerListModel.cpp
    application/LayerListModel.h
    application/PresetImageProvider.cpp
    application/PresetImageProvider.h
    panels/CanvasView.cpp
    panels/CanvasView.h)

add_library(Prism::ui ALIAS prism_ui)

target_link_libraries(prism_ui PUBLIC
    Prism::core
    Prism::canvas
    Prism::io
    Qt6::Quick)

target_include_directories(prism_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
