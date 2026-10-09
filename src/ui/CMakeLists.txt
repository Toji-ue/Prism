# prism_ui — QML-facing C++ bridge (controller, canvas view, layer model).
# Qt allowed here, but domain logic stays in Prism::core / Prism::canvas.
# Types are registered with qmlRegisterType in main.cpp.

add_library(prism_ui STATIC
    application/AppController.cpp
    application/AppController.h
    application/CollabSession.cpp
    application/CollabSession.h
    application/LayerListModel.cpp
    application/LayerListModel.h
    application/PresetImageProvider.cpp
    application/RecordExport.cpp
    application/SessionRecorder.cpp
    application/PresetImageProvider.h
    application/RecordExport.h
    application/SessionRecorder.h
    panels/CanvasView.cpp
    panels/CanvasView.h)

add_library(Prism::ui ALIAS prism_ui)

target_link_libraries(prism_ui PUBLIC
    Prism::core
    Prism::canvas
    Prism::io
    Qt6::Quick
    # LAN collab: a QTcpServer/QTcpSocket pair, no extra dependency beyond the
    # Qt Network module the deployment already ships.
    Qt6::Network)

target_include_directories(prism_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# MP4 export goes through the platform encoder (Media Foundation), so no codec
# ships with the app: bundling FFmpeg would mean either a 70 MB download or a
# GPL build, neither of which is acceptable here.
if(WIN32)
    target_link_libraries(prism_ui PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()
