
file(GLOB_RECURSE ALL_TEST_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(boost_requests_tests ${ALL_TEST_FILES})
target_link_libraries(boost_requests_tests PUBLIC Boost::requests Boost::filesystem rt)

target_compile_definitions(boost_requests_tests PUBLIC
        BOOST_REQUESTS_SEPARATE_COMPILATION=1
        BOOST_FILESYSTEM_NO_DEPRECATED=1)