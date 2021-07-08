include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

foreach(pkg_config_lib CAIRO)
    include_directories(${${pkg_config_lib}_INCLUDE_DIRS})
    link_directories(${${pkg_config_lib}_LIBRARY_DIRS})
endforeach()

if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    add_definitions(-DTEST_BUILD_ON_WINDOWS)
endif()

# test suite

set(testsuite_SOURCES
    harness.cpp
    analysis/contour_area/test.cpp
    core/expr/test.cpp
    core/locale/test.cpp
    core/path/test.cpp
    constraint/points_coincident/test.cpp
    constraint/pt_pt_distance/test.cpp
    constraint/pt_plane_distance/test.cpp
    constraint/pt_line_distance/test.cpp
    constraint/pt_face_distance/test.cpp
    constraint/proj_pt_distance/test.cpp
    constraint/pt_in_plane/test.cpp
    constraint/pt_on_line/test.cpp
    constraint/pt_on_face/test.cpp
    constraint/equal_length_lines/test.cpp
    constraint/length_ratio/test.cpp
    constraint/eq_len_pt_line_d/test.cpp
    constraint/eq_pt_ln_distances/test.cpp
    constraint/equal_angle/test.cpp
    constraint/equal_line_arc_len/test.cpp
    constraint/length_difference/test.cpp
    constraint/symmetric/test.cpp
    constraint/symmetric_horiz/test.cpp
    constraint/symmetric_vert/test.cpp
    constraint/symmetric_line/test.cpp
    constraint/at_midpoint/test.cpp
    constraint/horizontal/test.cpp
    constraint/vertical/test.cpp
    constraint/diameter/test.cpp
    constraint/pt_on_circle/test.cpp
    constraint/same_orientation/test.cpp
    constraint/angle/test.cpp
    constraint/parallel/test.cpp
    constraint/perpendicular/test.cpp
    constraint/arc_line_tangent/test.cpp
    constraint/cubic_line_tangent/test.cpp
    constraint/curve_curve_tangent/test.cpp
    constraint/equal_radius/test.cpp
    constraint/where_dragged/test.cpp
    constraint/comment/test.cpp
    request/arc_of_circle/test.cpp
    request/circle/test.cpp
    request/cubic/test.cpp
    request/cubic_periodic/test.cpp
    request/datum_point/test.cpp
    request/image/test.cpp
    request/line_segment/test.cpp
    request/ttf_text/test.cpp
    request/workplane/test.cpp
    group/link/test.cpp
    group/translate_asy/test.cpp
    group/translate_nd/test.cpp
)

add_executable(solvespace-testsuite
    ${testsuite_SOURCES}
    $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

target_link_libraries(solvespace-testsuite
    solvespace-headless
    ${COVERAGE_LIBRARY})

add_dependencies(solvespace-testsuite
    resources)

add_custom_target(test_solvespace
    COMMAND $<TARGET_FILE:solvespace-testsuite>
    COMMENT "Testing SolveSpace"
    VERBATIM)

# coverage reports

if(ENABLE_COVERAGE)
    set(LCOV_FLAGS   -q --gcov-tool ${GCOV})
    set(LCOV_FLAGS   ${LCOV_FLAGS} --rc lcov_branch_coverage=1)
    set(LCOV_FLAGS   ${LCOV_FLAGS} --rc "lcov_excl_line=(ssassert|switch)")
    set(LCOV_FLAGS   ${LCOV_FLAGS} --rc "lcov_excl_br_line=BRANCH_ALWAYS_TAKEN")
    set(LCOV_COLLECT -c -b ${CMAKE_SOURCE_DIR}/src -d ${CMAKE_BINARY_DIR}/src --no-external)

    add_custom_command(
        OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/coverage_base.info
        COMMAND ${LCOV} ${LCOV_FLAGS} ${LCOV_COLLECT}
            -o ${CMAKE_BINARY_DIR}/coverage_base.info -i
        DEPENDS solvespace-testsuite
        COMMENT "Importing baseline coverage data"
        VERBATIM)

    add_custom_target(coverage_solvespace ALL
        COMMAND ${LCOV} ${LCOV_FLAGS} ${LCOV_COLLECT}
            -o ${CMAKE_BINARY_DIR}/coverage_test.info
        COMMAND ${LCOV} ${LCOV_FLAGS}
            -o ${CMAKE_BINARY_DIR}/coverage_full.info
            -a ${CMAKE_BINARY_DIR}/coverage_base.info
            -a ${CMAKE_BINARY_DIR}/coverage_test.info
        COMMAND ${LCOV} ${LCOV_FLAGS} --summary
            ${CMAKE_BINARY_DIR}/coverage_full.info
        COMMAND ${GENHTML} -q --branch-coverage --demangle-cpp --legend
            ${CMAKE_BINARY_DIR}/coverage_full.info
            -o ${CMAKE_BINARY_DIR}/coverage/
            -t "SolveSpace testbench"
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/coverage_base.info
        DEPENDS test_solvespace
        COMMENT "Generating coverage report"
        VERBATIM)
endif()

# debug runner

set(debugtool_SOURCES
    debugtool.cpp
)

if (ENABLE_OPENMP)
    include_directories(${OpenMP_CXX_INCLUDE_DIRS})
endif()

add_executable(solvespace-debugtool
    ${debugtool_SOURCES}
    $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

target_link_libraries(solvespace-debugtool
    solvespace-core
    solvespace-headless)

add_dependencies(solvespace-debugtool
    resources)
