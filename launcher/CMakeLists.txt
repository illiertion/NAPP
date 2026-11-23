set(NAPP_LAUNCHER_SOURCES
    main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/LauncherBackend.cpp
    src/ui/LauncherBackend.h
    src/ui/MapListModel.cpp
    src/ui/MapListModel.h
    src/ui/Theme.cpp
    src/ui/Theme.h
    src/core/AppRegistry.cpp
    src/core/AppRegistry.h
    src/core/AppLauncher.cpp
    src/core/AppLauncher.h
    src/store/RepositoryService.cpp
    src/store/RepositoryService.h
    src/store/CatalogCache.cpp
    src/store/CatalogCache.h
    src/network/HttpClient.cpp
    src/network/HttpClient.h
    src/runtime/RuntimeManager.cpp
    src/runtime/RuntimeManager.h
    src/settings/Settings.cpp
    src/settings/Settings.h
    src/security/PermissionStore.cpp
    src/security/PermissionStore.h
    src/package/InstallService.cpp
    src/package/InstallService.h
    src/system/Notifier.cpp
    src/system/Notifier.h
    src/system/FileAssociation.cpp
    src/system/FileAssociation.h
    src/system/SingleInstance.cpp
    src/system/SingleInstance.h
    src/system/Shortcut.cpp
    src/system/Shortcut.h
)

qt_add_executable(napp-launcher WIN32 ${NAPP_LAUNCHER_SOURCES})
target_include_directories(napp-launcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(napp-launcher PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::Quick
    Qt6::QuickWidgets
    napp_core
    napp_ipkg
)

if(WIN32)
    target_link_libraries(napp-launcher PRIVATE shell32 ole32)
endif()

# Default (official) repository. Point this at your own raw URL when the
# repository is not hosted at ILLIERTION/NAPP-Repository.
target_compile_definitions(napp-launcher PRIVATE
    NAPP_OFFICIAL_REPOSITORY_URL="${NAPP_OFFICIAL_REPOSITORY_URL}")
napp_apply_warnings(napp-launcher)
set_target_properties(napp-launcher PROPERTIES OUTPUT_NAME "NAPP")

# Explorer icon for NAPP.exe, generated from the same PNG set as the window icon.
if(WIN32)
    target_sources(napp-launcher PRIVATE assets/napp.rc)
endif()

# All launcher screens are QML; Widgets only hosts the window and native dialogs.
qt_add_resources(napp-launcher "napp_qml"
    PREFIX "/qml"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/qml
    FILES
        qml/Main.qml
        qml/HomePage.qml
        qml/StorePage.qml
        qml/LibraryPage.qml
        qml/RuntimePage.qml
        qml/SettingsPage.qml
        qml/AboutPage.qml
        qml/components/qmldir
        qml/components/Theme.qml
        qml/components/I18n.qml
        qml/components/NappButton.qml
        qml/components/NappTextField.qml
        qml/components/NappComboBox.qml
        qml/components/NappCheckBox.qml
        qml/components/NappProgress.qml
        qml/components/AppCover.qml
        qml/components/SectionHeader.qml
        qml/components/EmptyState.qml
)

# Branding shipped inside the binary: the rail mark, the About screen and the
# window/exe icon all use the same source image.
qt_add_resources(napp-launcher "napp_branding"
    PREFIX "/branding"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/assets
    FILES
        assets/logo-256.png
        assets/napp-icon-256.png
)

# Development convenience: make the build-tree launcher runnable on its own.
find_program(NAPP_WINDEPLOYQT NAMES windeployqt HINTS "${Qt6_DIR}/../../../bin")
if(WIN32 AND NAPP_WINDEPLOYQT)
    # --qmldir makes windeployqt bundle the QtQuick/Controls/Layouts modules the
    # QML screens import; without it the launcher starts with an empty window.
    add_custom_command(TARGET napp-launcher POST_BUILD
        COMMAND "${NAPP_WINDEPLOYQT}" --no-translations --no-compiler-runtime
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" "$<TARGET_FILE:napp-launcher>"
        COMMENT "Deploying Qt runtime + QML modules for NAPP launcher"
    )
endif()
