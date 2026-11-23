set(CEF_ROOT "${PROJECT_SOURCE_DIR}/third_party/cef")
if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
    message(FATAL_ERROR
        "third_party/cef 가 없습니다. CEF 바이너리 배포물(windows64)을 third_party/cef 에 압축 해제하세요.")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
# Qt is built against the dynamic CRT, so the CEF wrapper and host must use /MD
# (CEF defaults to /MT).
set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "" FORCE)
find_package(CEF REQUIRED)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

set(NAPP_HOST_SOURCES
    main.cpp
    host_options.cpp
    host_options.h
    napp_scheme_handler.cpp
    napp_scheme_handler.h
    host_client.cpp
    host_client.h
    host_window.cpp
    host_window.h
    napp_environment.cpp
    napp_environment.h
)

# The binary distribution ships Release/libcef.lib; wrap it as an imported target.
add_library(napp_libcef STATIC IMPORTED GLOBAL)
set_target_properties(napp_libcef PROPERTIES
    IMPORTED_LOCATION "${CEF_ROOT}/Release/libcef.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CEF_ROOT}"
)

qt_add_executable(napp-runtime-host WIN32 ${NAPP_HOST_SOURCES})
target_include_directories(napp-runtime-host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CEF_INCLUDE_PATH})
target_link_libraries(napp-runtime-host PRIVATE
    Qt6::Widgets
    napp_core
    napp_libcef
    libcef_dll_wrapper
)
napp_apply_warnings(napp-runtime-host)
# CEF headers pull in <windows.h>; NOMINMAX keeps std::min/max usable.
target_compile_definitions(napp-runtime-host PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
)
if(MSVC)
    target_compile_options(napp-runtime-host PRIVATE /utf-8)
endif()

# Stage a self-contained, deployable runtime directory:
# host + Qt DLLs/plugins + CEF binaries/resources + runtime.json.
set(NAPP_RUNTIME_STAGE "${CMAKE_BINARY_DIR}/runtime-stage")
find_program(NAPP_WINDEPLOYQT NAMES windeployqt HINTS "${Qt6_DIR}/../../../bin")

add_custom_command(TARGET napp-runtime-host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${NAPP_RUNTIME_STAGE}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:napp-runtime-host>" "${NAPP_RUNTIME_STAGE}/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_ROOT}/Release" "${NAPP_RUNTIME_STAGE}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_ROOT}/Resources" "${NAPP_RUNTIME_STAGE}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/runtime.json" "${NAPP_RUNTIME_STAGE}/runtime.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/launcher/assets/napp.ico" "${NAPP_RUNTIME_STAGE}/napp.ico"
    COMMENT "Staging NAPP runtime package into ${NAPP_RUNTIME_STAGE}"
)

if(NAPP_WINDEPLOYQT)
    # Deploy Qt next to the staged host so the package is complete on its own.
    add_custom_command(TARGET napp-runtime-host POST_BUILD
        COMMAND "${NAPP_WINDEPLOYQT}" --no-translations --no-compiler-runtime
                "${NAPP_RUNTIME_STAGE}/napp-runtime-host.exe"
        COMMENT "Deploying Qt runtime into ${NAPP_RUNTIME_STAGE}"
    )
endif()
