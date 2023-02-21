cmake_minimum_required(VERSION 3.20)

set(BOOST_REQUESTS_VERSION 1)
if(BOOST_SUPERPROJECT_VERSION)
    set(BOOST_REQUESTS_VERSION ${BOOST_SUPERPROJECT_VERSION})
endif()

project(boost_requests VERSION "${BOOST_REQUESTS_VERSION}" LANGUAGES CXX)

set(BOOST_REQUESTS_IS_ROOT OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BOOST_REQUESTS_IS_ROOT ON)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../boost.css)
    set(BOOST_REQUESTS_SHOULD_BE_INLINE ON)
else()
    set(BOOST_REQUESTS_SHOULD_BE_INLINE OFF)
endif()

option(BOOST_REQUESTS_BUILD_INLINE "Configure as if part of the boost source tree" ${BOOST_REQUESTS_SHOULD_BE_INLINE})

if(NOT BOOST_SUPERPROJECT_VERSION)
    option(BOOST_REQUESTS_INSTALL "Install boost::requests files" ON)
    option(BOOST_REQUESTS_BUILD_TESTS "Build boost::requests tests" ${BUILD_TESTING})
    option(BOOST_REQUESTS_BUILD_EXAMPLES "Build boost::requests examples" ${BOOST_REQUESTS_IS_ROOT})
else()
    set(BOOST_REQUESTS_BUILD_TESTS ${BUILD_TESTING})
endif()


if(BOOST_REQUESTS_IS_ROOT AND BOOST_REQUESTS_BUILD_INLINE)
    #
    # Building inside Boost tree, but as a separate project e.g. on Travis or
    # other CI, or when producing Visual Studio Solution and Projects.
    set(BOOST_INCLUDE_LIBRARIES requests)
    set(BOOST_EXCLUDE_LIBRARIES requests)

    set(CMAKE_FOLDER _deps)
    add_subdirectory(../.. _deps/boost EXCLUDE_FROM_ALL)
    unset(CMAKE_FOLDER)
endif()

find_package(Threads REQUIRED)

if (NOT BOOST_REQUESTS_BUILD_INLINE)
    find_package(Boost REQUIRED atomic container filesystem system json url)
    include_directories(include)
endif()


#-------------------------------------------------------------------------------
#
# OpenSSL
#
#-------------------------------------------------------------------------------


if (APPLE AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
    find_program(HOMEBREW brew)
    if (NOT HOMEBREW STREQUAL "HOMEBREW-NOTFOUND")
        execute_process(COMMAND brew --prefix openssl
                OUTPUT_VARIABLE OPENSSL_ROOT_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
endif()

find_package(OpenSSL)
if (NOT MSVC)
    link_libraries(${OPENSSL_LIBRARIES})
endif()

add_library(boost_requests src/src.cpp)
target_include_directories(boost_requests PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(
        boost_requests
            PUBLIC
                Boost::beast
                Boost::container
                Boost::filesystem
                Boost::json
                Boost::system
                Boost::url
                OpenSSL::SSL
                OpenSSL::Crypto
                Threads::Threads)

target_compile_definitions(boost_requests PUBLIC BOOST_REQUESTS_SEPARATE_COMPILATION=1 BOOST_FILESYSTEM_NO_DEPRECATED=1)
add_library(Boost::requests ALIAS boost_requests)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(boost_requests PUBLIC BOOST_REQUESTS_DYN_LINK=1)
else()
    target_compile_definitions(boost_requests PUBLIC BOOST_REQUESTS_STATIC_LINK=1)
endif()

if(BOOST_REQUESTS_INSTALL AND NOT BOOST_SUPERPROJECT_VERSION)
    install(TARGETS boost_requests
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            )
endif()

if (OPENSSL_FOUND)
    include_directories (${OPENSSL_INCLUDE_DIR})
endif()

if(BOOST_REQUESTS_BUILD_TESTS)
    add_subdirectory(test)
endif()


if(BOOST_REQUESTS_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
