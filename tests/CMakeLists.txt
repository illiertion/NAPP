set(NAPP_FIXTURE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

napp_add_test(napp_test_core
    SOURCES test_core.cpp
    LIBS napp_core
)
target_compile_definitions(napp_test_core PRIVATE NAPP_FIXTURE_DIR="${NAPP_FIXTURE_DIR}")

napp_add_test(napp_test_ipkg
    SOURCES test_ipkg.cpp
    LIBS napp_core napp_ipkg
)
target_compile_definitions(napp_test_ipkg PRIVATE NAPP_FIXTURE_DIR="${NAPP_FIXTURE_DIR}")
