file(GLOB DOMAIN_SOURCES "*.cpp")

add_library(domain ${DOMAIN_SOURCES})

target_include_directories(domain PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/3dparty/faiss
    ${CMAKE_SOURCE_DIR}/3dparty/fastText/src
    ${CMAKE_SOURCE_DIR}/3dparty/iceoryx2
)

target_link_libraries(domain PUBLIC 
    faiss
    fasttext-shared
    spdlog
    core
    iceoryx2-cxx
)