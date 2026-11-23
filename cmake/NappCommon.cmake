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

# napp_add_version_resource(<target> DESCRIPTION <text> PRODUCT <name>
#                           ORIGINAL_FILENAME <file.exe> [INTERNAL_NAME <name>]
#                           [ICON <path.ico>] [EXECUTABLE])
#
# Generates a Windows VERSIONINFO resource (plus the exe icon) from
# cmake/version.rc.in and attaches it to <target>. This is what the Explorer
# file-properties dialog shows, and keeping it generated from PROJECT_VERSION
# stops the shipped version from drifting away from the one being built.
# No-op off Windows, so call sites stay unconditional.
function(napp_add_version_resource target)
    cmake_parse_arguments(ARG "EXECUTABLE"
        "DESCRIPTION;PRODUCT;ORIGINAL_FILENAME;INTERNAL_NAME;ICON" "" ${ARGN})

    if(NOT WIN32)
        return()
    endif()
    if(NOT ARG_DESCRIPTION OR NOT ARG_PRODUCT OR NOT ARG_ORIGINAL_FILENAME)
        message(FATAL_ERROR
            "napp_add_version_resource(${target}) needs DESCRIPTION, PRODUCT and ORIGINAL_FILENAME")
    endif()

    if(NOT ARG_ICON)
        set(ARG_ICON "${PROJECT_SOURCE_DIR}/launcher/assets/napp.ico")
    endif()
    if(NOT ARG_INTERNAL_NAME)
        get_filename_component(ARG_INTERNAL_NAME "${ARG_ORIGINAL_FILENAME}" NAME_WE)
    endif()

    set(NAPP_RC_FILEVERSION
        "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
    set(NAPP_RC_VERSION "${PROJECT_VERSION}")
    set(NAPP_RC_COMPANY "ILLIERTION")
    set(NAPP_RC_COPYRIGHT "Copyright (c) ILLIERTION")
    set(NAPP_RC_PRODUCT "${ARG_PRODUCT}")
    set(NAPP_RC_DESCRIPTION "${ARG_DESCRIPTION}")
    set(NAPP_RC_INTERNAL_NAME "${ARG_INTERNAL_NAME}")
    set(NAPP_RC_ORIGINAL_FILENAME "${ARG_ORIGINAL_FILENAME}")
    if(ARG_EXECUTABLE)
        set(NAPP_RC_FILETYPE "VFT_APP")
    else()
        set(NAPP_RC_FILETYPE "VFT_DLL")
    endif()
    # rc.exe resolves a quoted icon path relative to the .rc file, so the icon is
    # copied next to the generated resource and referenced by name. That also
    # keeps non-ASCII install paths out of the resource text.
    # rc.exe 는 따옴표 안의 아이콘 경로를 .rc 파일 기준으로 찾는다. 그래서 생성된
    # 리소스 옆에 아이콘을 복사하고 이름만 적는다. 경로에 한글이 있어도 안전하다.
    set(_napp_rc_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${_napp_rc_dir}")
    file(COPY "${ARG_ICON}" DESTINATION "${_napp_rc_dir}")
    get_filename_component(NAPP_RC_ICON "${ARG_ICON}" NAME)

    set(_napp_rc_out "${_napp_rc_dir}/${target}-version.rc")
    configure_file("${PROJECT_SOURCE_DIR}/cmake/version.rc.in" "${_napp_rc_out}" @ONLY)
    target_sources(${target} PRIVATE "${_napp_rc_out}")
endfunction()
