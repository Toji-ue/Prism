find_package(GTest CONFIG REQUIRED)

add_executable(core_tests
    TestBlend.cpp
    TestCompositor.cpp
    TestDocument.cpp
    TestGeometry.cpp
    TestHistory.cpp
    TestStrokeCommands.cpp)

target_link_libraries(core_tests PRIVATE
    Prism::core
    Prism::canvas
    GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(core_tests)
