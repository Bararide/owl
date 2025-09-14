add_library(core INTERFACE)

target_include_directories(core INTERFACE
    ${CMAKE_SOURCE_DIR}/3dparty/spdlog
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(core INTERFACE spdlog)