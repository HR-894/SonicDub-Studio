find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

add_executable(vd_tests
    test_pipeline.cpp
    test_audio_extractor.cpp
)

target_include_directories(vd_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vd_tests PRIVATE 
    GTest::gtest_main 
    GTest::gmock
    
    # Link against the same libs as main app (minus GUI)
    CURL::libcurl
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Boost::system Boost::beast
)

include(GoogleTest)
gtest_discover_tests(vd_tests)
