include_directories(
    ${CMAKE_SOURCE_DIR}/include)

add_executable(CDemo
    CDemo.c)

target_link_libraries(CDemo
    slvs)

if (ENABLE_OPENMP)
    include_directories(${OpenMP_CXX_INCLUDE_DIRS})
endif()