set(MINIZ_DIR ${PROJECT_SOURCE_DIR}/third_party/miniz)
if(NOT EXISTS ${MINIZ_DIR}/miniz.c)
    message(FATAL_ERROR "third_party/miniz/miniz.c 가 없습니다. 규격 문서의 의존성 절차를 확인하세요.")
endif()

add_library(napp_miniz STATIC
    ${MINIZ_DIR}/miniz.c
    ${MINIZ_DIR}/miniz_tdef.c
    ${MINIZ_DIR}/miniz_tinfl.c
    ${MINIZ_DIR}/miniz_zip.c
)
target_include_directories(napp_miniz PUBLIC ${MINIZ_DIR})
# miniz is third party code; do not apply NAPP warning flags to it.
if(MSVC)
    target_compile_options(napp_miniz PRIVATE /w)
    target_compile_definitions(napp_miniz PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(napp_miniz PRIVATE -w)
endif()
set_target_properties(napp_miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(NAPP_IPKG_HEADERS
    include/napp/ipkg/limits.h
    include/napp/ipkg/zip_reader.h
    include/napp/ipkg/path_rules.h
    include/napp/ipkg/manifest.h
    include/napp/ipkg/manifest_parser.h
    include/napp/ipkg/package_verifier.h
    include/napp/ipkg/installer.h
)

napp_add_library(napp_ipkg
    SOURCES
        src/zip_reader.cpp
        src/path_rules.cpp
        src/manifest_parser.cpp
        src/package_verifier.cpp
        src/installer.cpp
        ${NAPP_IPKG_HEADERS}
    LIBS
        napp_core
        napp_miniz
)
