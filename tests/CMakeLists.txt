find_package(GTest CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_executable(core_tests
    TestBlend.cpp
    TestColorMath.cpp
    TestCompositor.cpp
    TestDocument.cpp
    TestFloodFill.cpp
    TestGeometry.cpp
    TestHistory.cpp
    TestLayerCommands.cpp
    TestProjectIO.cpp
    TestSelection.cpp
    TestStrokeCommands.cpp)

target_link_libraries(core_tests PRIVATE
    Prism::core
    Prism::canvas
    Prism::io
    Qt6::Core
    Qt6::Gui
    GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(core_tests)
