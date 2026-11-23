qt_add_executable(napp-runtime-host stub_main.cpp)
target_link_libraries(napp-runtime-host PRIVATE Qt6::Core)
napp_apply_warnings(napp-runtime-host)
target_compile_definitions(napp-runtime-host PRIVATE NAPP_RUNTIME_STUB=1)
