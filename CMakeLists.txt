cmake_minimum_required(VERSION 3.16)
project(owl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(FUSE3 REQUIRED fuse3)
pkg_check_modules(LZ4 REQUIRED liblz4)

set(ZeroMQ_FOUND ${ZMQ_FOUND})
set(ZeroMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_DIRS})
set(ZeroMQ_LIBRARIES ${ZMQ_LIBRARIES})

find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
else()
    message(FATAL_ERROR "OpenMP is required but not found")
endif()

option(TBB_TEST "Enable TBB tests" OFF)
option(TBB_EXAMPLES "Enable TBB examples" OFF)
option(TBB_STRICT "Enable strict warnings" OFF)
option(TBB_CPF "Enable preview features of the library" OFF)

add_subdirectory(3dparty/tbb EXCLUDE_FROM_ALL)

set(FAISS_ENABLE_GPU OFF CACHE BOOL "Disable GPU support" FORCE)
set(FAISS_ENABLE_PYTHON OFF CACHE BOOL "Disable Python bindings" FORCE)
set(FAISS_ENABLE_C_API OFF CACHE BOOL "Disable C API" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable testing" FORCE)

find_package(OpenSSL REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(3dparty/faiss EXCLUDE_FROM_ALL)

set(PISTACHE_USE_SSL OFF CACHE BOOL "Disable SSL support" FORCE)
add_subdirectory(3dparty/pistache EXCLUDE_FROM_ALL)

add_subdirectory(3dparty/boost EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/sentencepiece EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/fastText EXCLUDE_FROM_ALL)

set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(3dparty/spdlog EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/libenvpp EXCLUDE_FROM_ALL)

add_subdirectory(lib)
add_subdirectory(domain)

add_executable(owl src/main.cpp)

target_compile_options(owl PRIVATE ${OpenMP_CXX_FLAGS})

target_include_directories(owl PRIVATE 
    ${FUSE3_INCLUDE_DIRS}     
    ${CMAKE_SOURCE_DIR}/3dparty/sentencepiece/src
    ${CMAKE_SOURCE_DIR}/3dparty/fastText/src
    ${JSONCPP_INCLUDE_DIRS}
    ${LZ4_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/lib/cppcore
    ${CMAKE_SOURCE_DIR}/lib/cppcore/infrastructure
    ${OPENSSL_INCLUDE_DIR}
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(owl TBB::tbb)
target_link_libraries(owl ${OpenMP_CXX_FLAGS})
target_link_libraries(owl faiss)
target_link_libraries(owl domain)
target_link_libraries(owl ossec)
target_link_libraries(owl chunkees)
target_link_libraries(owl cppcore)
target_link_libraries(owl libenvpp)
target_link_libraries(owl ${FUSE3_LIBRARIES})
target_link_libraries(owl sentencepiece)
target_link_libraries(owl fasttext-shared)
target_link_libraries(owl spdlog)
target_link_libraries(owl ${JSONCPP_LIBRARIES})
target_link_libraries(owl pistache)
target_link_libraries(owl Boost::url)
target_link_libraries(owl fmt::fmt)
target_link_libraries(owl ${LZ4_LIBRARIES})
target_link_libraries(owl OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(owl ${ZMQ_LIBRARIES})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(owl PRIVATE -mavx2 -mfma)
endif()