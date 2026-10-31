cmake_minimum_required(VERSION 3.21)

project(Plaza
        VERSION 1.1.1
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
        Quick
        QuickControls2
        Sql
        Concurrent
        Network
        DBus
)
find_package(ZLIB REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FLATPAK IMPORTED_TARGET flatpak)

if (NOT FLATPAK_FOUND)
    message(FATAL_ERROR
        "libflatpak development files were not found. Install them with:\n"
        "sudo dnf install flatpak-devel pkgconf-pkg-config"
    )
endif()

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(plaza
        main.cpp

        src/database/DatabaseController.h
        src/database/DatabaseController.cpp

        src/models/AppListModel.h
        src/models/AppListModel.cpp
        src/models/AppSearchModel.h
        src/models/AppSearchModel.cpp

        src/models/InstalledAppModel.h
        src/models/InstalledAppModel.cpp

        src/backends/FlatpakBackend.h
        src/backends/FlatpakBackend.cpp
        src/backends/GitHubUpdateController.h
        src/backends/GitHubUpdateController.cpp
        src/backends/FlatpakCatalog.h
        src/backends/FlatpakCatalog.cpp
        src/backends/PopularityProvider.h
        src/backends/PopularityProvider.cpp
        src/backends/PackageKitBackend.h
        src/backends/PackageKitBackend.cpp
)

qt_add_qml_module(plaza
        URI Plaza
        VERSION 1.0
        NO_IMPORT_SCAN

        QML_FILES
        Main.qml
)

qt_add_resources(plaza plaza_icons
        PREFIX "/"
        FILES icons/plaza.svg
)

qt_add_resources(plaza plaza_catalog
        PREFIX "/"
        FILES db/catalog.sql
)

target_link_libraries(plaza
        PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
        Qt6::Concurrent
        Qt6::Network
        Qt6::DBus
        ZLIB::ZLIB
        PkgConfig::FLATPAK
)

target_compile_definitions(plaza PRIVATE PLAZA_VERSION="${PROJECT_VERSION}")

install(TARGETS plaza
        RUNTIME DESTINATION bin
)
install(FILES plaza.desktop
        DESTINATION share/applications
)
install(FILES icons/plaza.svg
        DESTINATION share/icons/hicolor/scalable/apps
)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME plaza)
set(CPACK_PACKAGE_VENDOR Maxdemyd0)
set(CPACK_PACKAGE_CONTACT "Maxdemyd0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Plaza software center")
set(CPACK_PACKAGE_FILE_NAME "plaza_${PROJECT_VERSION}_${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Maxdemyd0")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "flatpak, libqt6sql6-sqlite, qml6-module-org-kde-kirigami, qml6-module-org-kde-desktop, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick, packagekit"
)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
    set(CPACK_PACKAGE_FILE_NAME "plaza_${PROJECT_VERSION}_amd64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
    set(CPACK_PACKAGE_FILE_NAME "plaza_${PROJECT_VERSION}_arm64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
    set(CPACK_PACKAGE_FILE_NAME "plaza_${PROJECT_VERSION}_i386")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv7l|armv7|armhf)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
    set(CPACK_PACKAGE_FILE_NAME "plaza_${PROJECT_VERSION}_armhf")
endif()
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "packagekit-backend-aptcc")
include(CPack)

include(CTest)
if (BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(app_search_test
        tests/AppSearchModelTest.cpp
        src/models/AppSearchModel.h
        src/models/AppSearchModel.cpp
    )
    target_link_libraries(app_search_test PRIVATE Qt6::Test Qt6::Gui)
    add_test(NAME app_search_test COMMAND app_search_test)
    qt_add_executable(flatpak_catalog_test
        tests/FlatpakCatalogTest.cpp
        src/database/DatabaseController.h
        src/database/DatabaseController.cpp
        src/backends/FlatpakCatalog.h
        src/backends/FlatpakCatalog.cpp
        src/models/AppListModel.h
        src/models/AppListModel.cpp
    )
    qt_add_resources(flatpak_catalog_test plaza_test_catalog
        PREFIX "/"
        FILES db/catalog.sql
    )
    target_link_libraries(flatpak_catalog_test PRIVATE Qt6::Test Qt6::Sql Qt6::Concurrent PkgConfig::FLATPAK ZLIB::ZLIB)
    add_test(NAME flatpak_catalog_test COMMAND flatpak_catalog_test)
    qt_add_executable(flatpak_backend_test
        tests/FlatpakBackendTest.cpp
        src/backends/FlatpakBackend.h
        src/backends/FlatpakBackend.cpp
    )
    target_link_libraries(flatpak_backend_test PRIVATE
        Qt6::Test Qt6::Network Qt6::Concurrent PkgConfig::FLATPAK
    )
    add_test(NAME flatpak_backend_test COMMAND flatpak_backend_test)
    qt_add_executable(github_update_controller_test
        tests/GitHubUpdateControllerTest.cpp
        src/backends/GitHubUpdateController.h
        src/backends/GitHubUpdateController.cpp
        src/backends/PackageKitBackend.h
        src/backends/PackageKitBackend.cpp
    )
    target_link_libraries(github_update_controller_test PRIVATE
        Qt6::Test Qt6::Network Qt6::DBus
    )
    add_test(NAME github_update_controller_test
        COMMAND dbus-run-session -- $<TARGET_FILE:github_update_controller_test>)
    qt_add_executable(packagekit_backend_test tests/PackageKitBackendTest.cpp
        src/backends/PackageKitBackend.h src/backends/PackageKitBackend.cpp)
    target_link_libraries(packagekit_backend_test PRIVATE Qt6::Test Qt6::DBus)
    add_test(NAME packagekit_backend_test COMMAND dbus-run-session -- $<TARGET_FILE:packagekit_backend_test>)
    qt_add_executable(popularity_test tests/PopularityTest.cpp
        src/backends/PopularityProvider.h src/backends/PopularityProvider.cpp
        src/models/AppSearchModel.h src/models/AppSearchModel.cpp)
    target_link_libraries(popularity_test PRIVATE Qt6::Test Qt6::Gui Qt6::Network)
    add_test(NAME popularity_test COMMAND popularity_test)
endif()
