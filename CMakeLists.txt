cmake_minimum_required(VERSION 3.16)
project(owl)

set(CMAKE_CXX_STANDARD 20)

set(FAISS_ENABLE_GPU OFF CACHE BOOL "Disable GPU support" FORCE)
set(FAISS_ENABLE_PYTHON OFF CACHE BOOL "Disable Python bindings" FORCE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

add_subdirectory(3dparty/boost EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/faiss EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/sentencepiece EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/fastText EXCLUDE_FROM_ALL)
add_subdirectory(3dparty/spdlog EXCLUDE_FROM_ALL)

include_directories(include)

add_executable(owl src/main.cpp)
target_include_directories(owl PRIVATE ${FUSE3_INCLUDE_DIRS}     
    ${CMAKE_SOURCE_DIR}/3dparty/sentencepiece/src
    ${CMAKE_SOURCE_DIR}/3dparty/fastText/src)

target_link_libraries(owl ${FUSE3_LIBRARIES})
target_link_libraries(owl faiss)
target_link_libraries(owl sentencepiece)
target_link_libraries(owl fasttext-shared)
target_link_libraries(owl spdlog)