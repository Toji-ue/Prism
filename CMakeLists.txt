cmake_minimum_required(VERSION 3.24)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/PrismVersion.txt" PRISM_VERSION LIMIT_COUNT 1)

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

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Test Network)
qt_standard_project_setup()

# --------------------------------------------------------------------------- #
# prism_core — framework-agnostic domain logic (no Qt/GPU types allowed)
add_subdirectory(src/core)

# prism_canvas — CPU compositing + brush (reference implementation)
add_subdirectory(src/canvas)

# prism_io — project file format + image import/export
add_subdirectory(src/io)

# prism_ui — QML bridge (controller, canvas view)
add_subdirectory(src/ui)

# --------------------------------------------------------------------------- #
# prism app
qt_add_executable(prism
    src/app/main.cpp)
# WIN32 GUI subsystem: no console window attaches when launched from Explorer.
set_target_properties(prism PROPERTIES WIN32_EXECUTABLE TRUE)

# Native staging is a separate preset; Windows uses its hash/license-gated scripts.
option(PRISM_STAGE_NATIVE_RUNTIME "Stage the macOS/Linux runtime for release review" OFF)
if(PRISM_STAGE_NATIVE_RUNTIME)
    if(WIN32)
        message(FATAL_ERROR "Use tools/deploy.ps1 and tools/package.ps1 on Windows")
    endif()
    include(GNUInstallDirs)
    if(APPLE)
        string(REGEX REPLACE "-.*$" "" PRISM_BUNDLE_VERSION "${PRISM_VERSION}")
        set_target_properties(prism PROPERTIES
            MACOSX_BUNDLE TRUE
            OUTPUT_NAME Prism
            MACOSX_BUNDLE_BUNDLE_NAME Prism
            MACOSX_BUNDLE_GUI_IDENTIFIER io.github.toji-ue.prism
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PRISM_BUNDLE_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PRISM_BUNDLE_VERSION}")
    else()
        set_target_properties(prism PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()
endif()

# Runtime theme switching: one shared QML singleton instance.
set_source_files_properties(src/ui/qml/ThemeManager.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(prism
    URI Prism
    VERSION 1.0
    QML_FILES
        src/ui/qml/ThemeManager.qml
        src/ui/qml/Icon.qml
        src/ui/qml/PSlider.qml
        src/ui/qml/PButton.qml
        src/ui/qml/PTextField.qml
        src/ui/qml/PCheckBox.qml
        src/ui/qml/NumericField.qml
        src/ui/qml/BrushSection.qml
        src/ui/qml/FocusScroll.qml
        src/ui/qml/PSpinBox.qml
        src/ui/qml/PComboBox.qml
        src/ui/qml/PMenu.qml
        src/ui/qml/Main.qml
        src/ui/qml/RecordingDocker.qml
        src/ui/qml/RecordingExportDialog.qml
        src/ui/qml/StartPage.qml
        src/ui/qml/FocusGlassPanel.qml
        src/ui/qml/QuickControls.qml
        src/ui/qml/CompactColour.qml
        src/ui/qml/ToolBox.qml
        src/ui/qml/ToolbarButton.qml
        src/ui/qml/ColorSelector.qml
        src/ui/qml/LayersDocker.qml
        src/ui/qml/BrushPresetsDocker.qml
        src/ui/qml/BrushLabSlider.qml
        src/ui/qml/NewDocumentDialog.qml)

target_link_libraries(prism PRIVATE
    Prism::core
    Prism::canvas
    Prism::ui
    Qt6::Quick
    # Qt Test is used only by the --demo-* verification hooks: they drive real
    # mouse events through the platform input path (QTest::mousePress/Move),
    # which is the only way to catch grab-dependent UI bugs like drag &
    # drop. Raw QCoreApplication::sendEvent bypasses that bookkeeping and
    # reports a working drag as broken.
    Qt6::Test)
target_compile_definitions(prism PRIVATE PRISM_VERSION="${PRISM_VERSION}")
target_compile_definitions(prism_ui PRIVATE PRISM_VERSION="${PRISM_VERSION}")

if(PRISM_STAGE_NATIVE_RUNTIME)
    install(TARGETS prism BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    # An unsupported deployment platform must fail rather than emit a bare executable.
    qt_generate_deploy_qml_app_script(TARGET prism OUTPUT_SCRIPT prism_deploy_script NO_TRANSLATIONS)
    install(SCRIPT ${prism_deploy_script})
endif()

# --------------------------------------------------------------------------- #
# tests
enable_testing()
add_subdirectory(tests)
