include_directories(
    ${CMAKE_SOURCE_DIR}/include)

add_executable(CDemo
    CDemo.c)

target_link_libraries(CDemo
    slvs)

if(EMSCRIPTEN)
    set_target_properties(CDemo PROPERTIES
        LINK_FLAGS "-s TOTAL_MEMORY=134217728")
endif()
