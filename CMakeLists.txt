cmake_minimum_required(VERSION 3.21)

include(cmake/LpmMetadata.cmake)
project(${LILYGO_UI_CMAKE_PROJECT_NAME} LANGUAGES C CXX)

include(GNUInstallDirs)
include(CTest)
find_package(LilyGoUI CONFIG REQUIRED)
find_package(Threads REQUIRED)

configure_file(
    cmake/app_identity.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/app_identity.h
    @ONLY)

add_library(lilygo_ui_music_core STATIC
    src/pages/music/music_model.cpp
    src/pages/music/audio_backend.cpp)
target_include_directories(lilygo_ui_music_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_options(lilygo_ui_music_core PRIVATE
    -Wall -Wextra -Wpedantic)
target_compile_features(lilygo_ui_music_core PUBLIC cxx_std_17)
target_compile_definitions(lilygo_ui_music_core PRIVATE
    LILYGO_MUSIC_ASSET_DIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}/lilygo-ui-music"
    LILYGO_MUSIC_SOURCE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(lilygo_ui_music_core PUBLIC Threads::Threads)

add_library(lilygo_ui_music_ui STATIC
    src/app.cpp
    src/components/components.cpp
    src/pages/music/music_view.cpp
    src/pages/music/music_view_model.cpp)
target_include_directories(lilygo_ui_music_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_options(lilygo_ui_music_ui PRIVATE
    -Wall -Wextra -Wpedantic)
target_link_libraries(lilygo_ui_music_ui PUBLIC
    lilygo_ui_music_core
    LilyGoUI::Runtime)

add_executable(lilygo_ui_music src/main.cpp)
target_compile_options(lilygo_ui_music PRIVATE
    -Wall -Wextra -Wpedantic)
target_link_libraries(lilygo_ui_music PRIVATE lilygo_ui_music_ui)
set_target_properties(lilygo_ui_music PROPERTIES
    OUTPUT_NAME "${LILYGO_UI_EXECUTABLE_NAME}")

configure_file(data/launcher.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.json @ONLY)
configure_file(data/application.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.desktop @ONLY)
configure_file(data/application.metainfo.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.metainfo.xml @ONLY)

install(TARGETS lilygo_ui_music
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT app)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.json
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/launcher/apps
    COMPONENT app)
lilygo_ui_install_launcher_icon(
    "${LILYGO_UI_APP_ID}"
    "${CMAKE_CURRENT_SOURCE_DIR}/${LILYGO_UI_APP_ICON}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    COMPONENT app)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
    COMPONENT app)
lilygo_ui_install_font_licenses(
    "${CMAKE_INSTALL_DATAROOTDIR}/doc/${LILYGO_UI_PACKAGE_NAME}"
    app)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(music-core-tests tests/test_core.cpp)
    target_compile_options(music-core-tests PRIVATE
        -Wall -Wextra -Wpedantic -UNDEBUG)
    target_link_libraries(music-core-tests PRIVATE
        lilygo_ui_music_core)
    add_test(NAME music-core COMMAND music-core-tests)

    add_executable(music-view-model-tests tests/test_view_model.cpp)
    target_compile_options(music-view-model-tests PRIVATE
        -Wall -Wextra -Wpedantic -UNDEBUG)
    target_link_libraries(music-view-model-tests PRIVATE
        lilygo_ui_music_ui)
    add_test(NAME music-view-model COMMAND music-view-model-tests)

    add_executable(music-render-ui tests/render_ui.cpp)
    target_compile_options(music-render-ui PRIVATE
        -Wall -Wextra -Wpedantic -UNDEBUG)
    target_link_libraries(music-render-ui PRIVATE
        lilygo_ui_music_ui)
    foreach(viewport IN ITEMS portrait landscape)
        if(viewport STREQUAL "portrait")
            set(_width 568)
            set(_height 1232)
        else()
            set(_width 1232)
            set(_height 568)
        endif()
        add_test(NAME music-render-${viewport} COMMAND music-render-ui
            ${_width} ${_height}
            ${CMAKE_CURRENT_BINARY_DIR}/music-${viewport}.ppm)
    endforeach()
endif()

# New applications have no legacy CM0 package to replace. Add Debian migration
# fields only when this project supersedes an actually published package.
set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME "${LILYGO_UI_PACKAGE_NAME}")
set(CPACK_PACKAGE_VERSION "${LILYGO_UI_VERSION}")
if(LILYGO_UI_VERSION MATCHES
   "^([0-9]+:)?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(CPACK_PACKAGE_VERSION_MAJOR "${CMAKE_MATCH_2}")
    set(CPACK_PACKAGE_VERSION_MINOR "${CMAKE_MATCH_3}")
    set(CPACK_PACKAGE_VERSION_PATCH "${CMAKE_MATCH_4}")
else()
    set(CPACK_PACKAGE_VERSION_MAJOR 0)
    set(CPACK_PACKAGE_VERSION_MINOR 0)
    set(CPACK_PACKAGE_VERSION_PATCH 0)
endif()
set(CPACK_PACKAGE_CONTACT "${LILYGO_UI_PACKAGE_CONTACT}")
set(CPACK_PACKAGE_VENDOR "${LILYGO_UI_PACKAGE_CONTACT}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${LILYGO_UI_APP_SUMMARY}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${LILYGO_UI_APP_HOMEPAGE}")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_SET_DESTDIR ON)
set(CPACK_COMPONENTS_ALL app)
set(CPACK_DEBIAN_APP_PACKAGE_NAME "${LILYGO_UI_PACKAGE_NAME}")
set(CPACK_DEBIAN_APP_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_APP_PACKAGE_DEPENDS
    "libc6, libstdc++6, libdrm2, libfreetype6, mpv, lilygo-ui-appkit-dev (>= ${LILYGO_UI_MIN_APPKIT_VERSION})")
set(CPACK_DEBIAN_APP_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE
        "${LILYGO_UI_PACKAGE_ARCHITECTURE}")
endif()
include(CPack)
