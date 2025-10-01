cmake_minimum_required(VERSION 3.16)
project(owl)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAPNP REQUIRED capnp)
pkg_check_modules(KJ REQUIRED kj)

pkg_check_modules(CAPNP_RPC REQUIRED capnp-rpc)

message(STATUS "Found Cap'n Proto: ${CAPNP_VERSION}")
message(STATUS "Cap'n Proto include dirs: ${CAPNP_INCLUDE_DIRS}")
message(STATUS "Cap'n Proto libraries: ${CAPNP_LIBRARIES}")

pkg_check_modules(JSONCPP REQUIRED jsoncpp)

set(FAISS_ENABLE_GPU OFF CACHE BOOL "Disable GPU support" FORCE)
set(FAISS_ENABLE_PYTHON OFF CACHE BOOL "Disable Python bindings" FORCE)

set(BUILD_TESTING OFF CACHE BOOL "Disable testing" FORCE)
set(USE_SYSTEM_GTEST OFF CACHE BOOL "Disable system gtest" FORCE)

pkg_check_modules(FUSE3 REQUIRED fuse3)
pkg_check_modules(LZ4 REQUIRED liblz4)

find_package(Drogon REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(3dparty/boost EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/faiss EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/sentencepiece EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/fastText EXCLUDE_FROM_ALL)

set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(3dparty/spdlog EXCLUDE_FROM_ALL)

add_subdirectory(core)
add_subdirectory(domain)

include_directories(${JSONCPP_INCLUDE_DIRS})

set(CAPNPC_OUTPUT_DIR "${CMAKE_BINARY_DIR}/capnp_generated")
file(MAKE_DIRECTORY ${CAPNPC_OUTPUT_DIR})

find_program(CAPNP_TOOL capnp)
if(NOT CAPNP_TOOL)
    message(FATAL_ERROR "capnp tool not found")
endif()

find_program(CAPNPC_CXX_TOOL capnpc-c++)
if(NOT CAPNPC_CXX_TOOL)
    message(FATAL_ERROR "capnpc-c++ tool not found")
endif()

message(STATUS "Found capnp tool: ${CAPNP_TOOL}")
message(STATUS "Found capnpc-c++ tool: ${CAPNPC_CXX_TOOL}")

function(ADD_CAPNP_SCHEMA SCHEMA_FILE)
    get_filename_component(SCHEMA_NAME ${SCHEMA_FILE} NAME_WE)
    get_filename_component(SCHEMA_DIR ${SCHEMA_FILE} DIRECTORY)
    
    set(GENERATED_CPP "${CAPNPC_OUTPUT_DIR}/${SCHEMA_NAME}.capnp.c++")
    set(GENERATED_H "${CAPNPC_OUTPUT_DIR}/${SCHEMA_NAME}.capnp.h")
    
    add_custom_command(
        OUTPUT ${GENERATED_CPP} ${GENERATED_H}
        COMMAND ${CAPNP_TOOL} compile -o ${CAPNPC_CXX_TOOL} --src-prefix=${SCHEMA_DIR} ${SCHEMA_FILE}
        DEPENDS ${SCHEMA_FILE}
        WORKING_DIRECTORY ${CAPNPC_OUTPUT_DIR}
        COMMENT "Generating Cap'n Proto code for ${SCHEMA_FILE}"
        VERBATIM
    )
    
    set(CAPNP_GENERATED_SOURCES 
        ${CAPNP_GENERATED_SOURCES} 
        ${GENERATED_CPP}
        PARENT_SCOPE
    )

    include_directories(${CAPNPC_OUTPUT_DIR})
endfunction()

ADD_CAPNP_SCHEMA(${CMAKE_SOURCE_DIR}/domain/network/schemas/vectorfs.capnp)

add_executable(owl src/main.cpp ${CAPNP_GENERATED_SOURCES})

target_include_directories(owl PRIVATE 
    ${FUSE3_INCLUDE_DIRS}     
    ${CMAKE_SOURCE_DIR}/3dparty/sentencepiece/src
    ${CMAKE_SOURCE_DIR}/3dparty/fastText/src
    ${JSONCPP_INCLUDE_DIRS}
    ${LZ4_INCLUDE_DIRS}
    ${CAPNP_INCLUDE_DIRS}
    ${CAPNPC_OUTPUT_DIR})

target_link_libraries(owl domain)
target_link_libraries(owl core)
target_link_libraries(owl ${FUSE3_LIBRARIES})
target_link_libraries(owl faiss)
target_link_libraries(owl sentencepiece)
target_link_libraries(owl fasttext-shared)
target_link_libraries(owl spdlog)
target_link_libraries(owl ${JSONCPP_LIBRARIES})
target_link_libraries(owl Drogon::Drogon)
target_link_libraries(owl Boost::url)
target_link_libraries(owl fmt::fmt)
target_link_libraries(owl ${LZ4_LIBRARIES})

target_link_libraries(owl ${CAPNP_RPC_LIBRARIES} ${CAPNP_LIBRARIES} ${KJ_LIBRARIES})

target_compile_options(owl PRIVATE ${CAPNP_CFLAGS})