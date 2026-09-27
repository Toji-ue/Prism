find_package(GTest CONFIG REQUIRED)

add_executable(core_tests
    TestDocument.cpp
    TestHistory.cpp
    TestGeometry.cpp)

target_link_libraries(core_tests PRIVATE
    Prism::core
    GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(core_tests)
