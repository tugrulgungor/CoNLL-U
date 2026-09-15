# # # # # # # # # # # # # # # # # # # # # # # # # # #
# CoNLL-U - https://github.com/tugrulgungor/CoNLL-U #
#                                                   #
# Copyright (c) 2026. All rights reserved.          #
# Tuğrul Güngör - https://conll-u.tugrulgungor.me   #
#                                                   #
# Distributed under the MIT License.                #
# https://opensource.org/license/mit/               #
# # # # # # # # # # # # # # # # # # # # # # # # # # #

# Cmake configuration
cmake_minimum_required(VERSION 3.30)

# Project definition
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.version" CoNLL_U_VERSION_STRING)
string(STRIP "${CoNLL_U_VERSION_STRING}" CoNLL_U_VERSION_STRING)

project(CoNLL-U
        VERSION ${CoNLL_U_VERSION_STRING}
        LANGUAGES CXX
        HOMEPAGE_URL "https://conll-u.tugrulgungor.me"
)

# Project directories
set(CoNLL_U_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CoNLL_U_INCLUDE_DIR ${CoNLL_U_ROOT_DIR}/include)

# Library
set(CoNLL_U_LIBRARY_NAME ${PROJECT_NAME})
set(CoNLL_U_COMPILE_DEFINITIONS "")

file(GLOB_RECURSE CoNLL_U_LIBRARY_HEADERS ${CoNLL_U_INCLUDE_DIR}/*.*)

add_library(${CoNLL_U_LIBRARY_NAME} INTERFACE)

target_include_directories(${CoNLL_U_LIBRARY_NAME}
        INTERFACE
        $<BUILD_INTERFACE:${CoNLL_U_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${CoNLL_U_LIBRARY_NAME} INTERFACE cxx_std_23)

# Docs
set(CoNLL_U_DOCS_TARGET_NAME ${CoNLL_U_LIBRARY_NAME}-docs)
set(CoNLL_U_DOCS_DIR ${CoNLL_U_ROOT_DIR}/docs)
set(CoNLL_U_DOCS_CONFIG_FILE_OUT ${CoNLL_U_DOCS_DIR}/Doxyfile)
set(CoNLL_U_DOCS_CONFIG_FILE_IN ${CoNLL_U_ROOT_DIR}/Doxyfile.in)

if (BUILD_DOCS)
    find_package(Doxygen REQUIRED)

    include(FetchContent)

    FetchContent_Declare(
            doxygen-awesome-css
            GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
            GIT_TAG v2.4.2
    )
    FetchContent_MakeAvailable(doxygen-awesome-css)
    FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR DOXYGEN_AWESOME_CSS_DIR)

    file(MAKE_DIRECTORY ${CoNLL_U_DOCS_DIR})
    configure_file(${CoNLL_U_DOCS_CONFIG_FILE_IN} ${CoNLL_U_DOCS_CONFIG_FILE_OUT} @ONLY)

    doxygen_add_docs(${CoNLL_U_DOCS_TARGET_NAME}
            CONFIG_FILE ${CoNLL_U_DOCS_CONFIG_FILE_OUT}
            ALL
    )
endif ()

# Tests
set(CoNLL_U_TESTS_DIR ${CoNLL_U_ROOT_DIR}/tests)
set(CoNLL_U_TESTS_DATA_DIR ${CoNLL_U_TESTS_DIR}/.data)

if (BUILD_TESTS)
    find_package(GTest REQUIRED)

    enable_testing()
    include(GoogleTest)

    file(GLOB CoNLL_U_TEST_MODULES LIST_DIRECTORIES true ${CoNLL_U_TESTS_DIR}/*)

    foreach (CoNLL_U_TEST_MODULE_PATH ${CoNLL_U_TEST_MODULES})
        if (IS_DIRECTORY "${CoNLL_U_TEST_MODULE_PATH}")
            get_filename_component(CoNLL_U_TEST_MODULE_NAME "${CoNLL_U_TEST_MODULE_PATH}" NAME)

            file(GLOB_RECURSE CoNLL_U_TEST_MODULE_TARGETS ${CoNLL_U_TEST_MODULE_PATH}/*.cpp)
            if ("${CoNLL_U_TEST_MODULE_TARGETS}" STREQUAL "")
                continue()
            endif ()

            set(CoNLL_U_TEST_TARGET_NAME "${CoNLL_U_LIBRARY_NAME}-${CoNLL_U_TEST_MODULE_NAME}-tests")

            add_executable(${CoNLL_U_TEST_TARGET_NAME} ${CoNLL_U_TEST_MODULE_TARGETS})
            target_include_directories(${CoNLL_U_TEST_TARGET_NAME} PRIVATE ${CoNLL_U_INCLUDE_DIR})
            target_link_libraries(${CoNLL_U_TEST_TARGET_NAME} PRIVATE ${CoNLL_U_LIBRARY_NAME} GTest::gtest GTest::gtest_main)
            target_compile_definitions(${CoNLL_U_TEST_TARGET_NAME} PRIVATE -DCoNLL_U_TESTS_DATA_DIR="${CoNLL_U_TESTS_DATA_DIR}")

            gtest_discover_tests(${CoNLL_U_TEST_TARGET_NAME})
        endif ()
    endforeach ()
endif ()

# Demo
set(CoNLL_U_DEMO_DIR ${CoNLL_U_ROOT_DIR}/demo)
set(CoNLL_U_DEMO_TARGET_NAME ${CoNLL_U_LIBRARY_NAME}-demo)

if (BUILD_DEMO AND EMSCRIPTEN)
    add_executable(${CoNLL_U_DEMO_TARGET_NAME} ${CoNLL_U_DEMO_DIR}/demo.cpp)
    target_include_directories(${CoNLL_U_DEMO_TARGET_NAME} PRIVATE ${CoNLL_U_INCLUDE_DIR})
    target_link_libraries(${CoNLL_U_DEMO_TARGET_NAME} PRIVATE ${CoNLL_U_LIBRARY_NAME})

    target_link_options(${CoNLL_U_DEMO_TARGET_NAME} PRIVATE
            "--bind"
            "-sMODULARIZE=1"
            "-sEXPORT_NAME=createConlluDemoModule"
            "-sENVIRONMENT=web"
            "-sALLOW_MEMORY_GROWTH=1"
    )

    set_target_properties(${CoNLL_U_DEMO_TARGET_NAME} PROPERTIES
            OUTPUT_NAME "conllu"
            RUNTIME_OUTPUT_DIRECTORY ${CoNLL_U_DEMO_DIR}/.dist
    )
endif ()

# Installation
include(GNUInstallDirs)

install(DIRECTORY ${CoNLL_U_INCLUDE_DIR}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS ${CoNLL_U_LIBRARY_NAME}
        EXPORT "${CoNLL_U_LIBRARY_NAME}Targets"
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT "${CoNLL_U_LIBRARY_NAME}Targets"
        FILE "${CoNLL_U_LIBRARY_NAME}Targets.cmake"
        NAMESPACE tg::conllu
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CoNLL_U_LIBRARY_NAME}
)
