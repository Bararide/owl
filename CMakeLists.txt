cmake_minimum_required(VERSION 3.16)
project(owl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Убрать статическую линковку, она может вызывать проблемы
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

set(FAISS_ENABLE_GPU OFF CACHE BOOL "Disable GPU support" FORCE)
set(FAISS_ENABLE_PYTHON OFF CACHE BOOL "Disable Python bindings" FORCE)
set(FAISS_ENABLE_C_API OFF CACHE BOOL "Disable C API" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable testing" FORCE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(FUSE3 REQUIRED fuse3)
pkg_check_modules(LZ4 REQUIRED liblz4)

# Найти OpenSSL
find_package(OpenSSL REQUIRED)

find_package(fmt REQUIRED)

add_subdirectory(3dparty/faiss EXCLUDE_FROM_ALL)

# Отключить SSL в Pistache
set(PISTACHE_USE_SSL OFF CACHE BOOL "Disable SSL support" FORCE)
add_subdirectory(3dparty/pistache EXCLUDE_FROM_ALL)

add_subdirectory(3dparty/boost EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/sentencepiece EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/fastText EXCLUDE_FROM_ALL)

set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(3dparty/spdlog EXCLUDE_FROM_ALL)

add_subdirectory(lib)
add_subdirectory(domain)

add_executable(owl src/main.cpp)

target_include_directories(owl PRIVATE 
    ${FUSE3_INCLUDE_DIRS}     
    ${CMAKE_SOURCE_DIR}/3dparty/sentencepiece/src
    ${CMAKE_SOURCE_DIR}/3dparty/fastText/src
    ${JSONCPP_INCLUDE_DIRS}
    ${LZ4_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/lib/cppcore
    ${CMAKE_SOURCE_DIR}/lib/cppcore/infrastructure
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(owl faiss)
target_link_libraries(owl domain)
target_link_libraries(owl ossec)
target_link_libraries(owl chunkees)
target_link_libraries(owl cppcore)
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

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(owl PRIVATE -mavx2 -mfma)
endif()