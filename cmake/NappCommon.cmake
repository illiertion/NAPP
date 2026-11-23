# Shared build helpers for every NAPP target.

function(napp_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:preprocessor /utf-8 /EHsc)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
    endif()
endfunction()

# napp_add_library(<name> <sources...>)
# Creates a static library with public include/ directory and Qt Core linkage.
function(napp_add_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_library(${name} STATIC ${ARG_SOURCES})
    target_include_directories(${name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${name} PUBLIC Qt6::Core ${ARG_LIBS})
    napp_apply_warnings(${name})
    set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

# napp_add_test(<name> <sources...>) — registers a Qt Test executable with ctest.
function(napp_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    napp_apply_warnings(${name})
    # Console subsystem so Qt Test output reaches the terminal / ctest log.
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE FALSE)
    # Qt DLLs live outside the build tree on Windows; put them on PATH so the
    # test executable can start from ctest.
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND} -E env "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
                $<TARGET_FILE:${name}>)
endfunction()
