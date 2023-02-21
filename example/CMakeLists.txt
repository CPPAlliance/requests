
file(GLOB_RECURSE ALL_EXAMPLES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

include(FetchContent)
FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG        master)

FetchContent_MakeAvailable(pugixml)


foreach(SRC ${ALL_EXAMPLES})
    get_filename_component(NAME ${SRC} NAME_WLE )
    add_executable(boost_requests_example_${NAME} ${SRC} )

    target_link_libraries(boost_requests_example_${NAME} PUBLIC
                          OpenSSL::Crypto OpenSSL::SSL
                          Boost::requests
                          Boost::filesystem
                          pugixml::static)
    target_compile_definitions(boost_requests_example_${NAME} PUBLIC BOOST_REQUESTS_SEPARATE_COMPILATION=1 BOOST_FILESYSTEM_NO_DEPRECATED=1)
endforeach()
