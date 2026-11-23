# NAPP Web Runtime host.
# With NAPP_ENABLE_CEF=ON and third_party/cef extracted, this builds the real
# Chromium (CEF) host. Otherwise a stub is built so the launcher still links and
# reports the missing runtime instead of silently failing to start apps.
if(NAPP_ENABLE_CEF)
    add_subdirectory(cef-host)
else()
    add_subdirectory(stub)
endif()
