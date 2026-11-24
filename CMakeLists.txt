cmake_minimum_required(VERSION 3.25)

project(HyprEndfield VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network DBus WebSockets Qml Quick OpenGL ShaderTools Concurrent Sql Multimedia Pdf WebEngineWidgets)
find_package(LayerShellQt REQUIRED)
# 仅 KDE 下用到的桌面级快捷键；非 KDE 桌面不注册。
find_package(KF6GlobalAccel REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)
pkg_check_modules(VTERM REQUIRED IMPORTED_TARGET vterm>=0.3.3)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
pkg_check_modules(PAM REQUIRED IMPORTED_TARGET pam)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-unix-2.0)

find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner REQUIRED)
find_file(WLR_DATA_CONTROL_PROTOCOL wlr-data-control-unstable-v1.xml
    PATHS
        /usr/share/qt6/wayland/protocols/wlr-data-control
        /usr/share/wlr-protocols/unstable
    REQUIRED)
set(WLR_DATA_CONTROL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/wlr-data-control-unstable-v1-client-protocol.h")
set(WLR_DATA_CONTROL_SERVER_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/wlr-data-control-unstable-v1-server-protocol.h")
set(WLR_DATA_CONTROL_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/wlr-data-control-unstable-v1-protocol.c")
add_custom_command(
    OUTPUT "${WLR_DATA_CONTROL_HEADER}" "${WLR_DATA_CONTROL_SOURCE}" "${WLR_DATA_CONTROL_SERVER_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${WLR_DATA_CONTROL_PROTOCOL}" "${WLR_DATA_CONTROL_HEADER}"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" server-header "${WLR_DATA_CONTROL_PROTOCOL}" "${WLR_DATA_CONTROL_SERVER_HEADER}"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${WLR_DATA_CONTROL_PROTOCOL}" "${WLR_DATA_CONTROL_SOURCE}"
    DEPENDS "${WLR_DATA_CONTROL_PROTOCOL}"
    VERBATIM)


# KWin 只实现 ext-data-control-v1，wlroots 系合成器用 wlr-data-control。
# 两套协议结构几乎一致，运行期按 global 选择，故两套都生成。
find_file(EXT_DATA_CONTROL_PROTOCOL ext-data-control-v1.xml
    PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/protocols"
        /usr/share/wayland-protocols/staging/ext-data-control
    REQUIRED)
set(EXT_DATA_CONTROL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/ext-data-control-v1-client-protocol.h")
set(EXT_DATA_CONTROL_SERVER_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/ext-data-control-v1-server-protocol.h")
set(EXT_DATA_CONTROL_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/ext-data-control-v1-protocol.c")
add_custom_command(
    OUTPUT "${EXT_DATA_CONTROL_HEADER}" "${EXT_DATA_CONTROL_SOURCE}" "${EXT_DATA_CONTROL_SERVER_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${EXT_DATA_CONTROL_PROTOCOL}" "${EXT_DATA_CONTROL_HEADER}"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" server-header "${EXT_DATA_CONTROL_PROTOCOL}" "${EXT_DATA_CONTROL_SERVER_HEADER}"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${EXT_DATA_CONTROL_PROTOCOL}" "${EXT_DATA_CONTROL_SOURCE}"
    DEPENDS "${EXT_DATA_CONTROL_PROTOCOL}"
    VERBATIM)

qt_standard_project_setup(REQUIRES 6.8)

set(EVENT_LOG_SOURCES
    src/eventlogcontroller.cpp src/eventlogcontroller.hpp src/eventlogtext.cpp
    src/endfield/modules/event_log.cpp src/endfield/core/data/event_log_store.cpp
    src/endfield/core/data/json.cpp src/endfield/core/data/file_io.cpp src/endfield/data_platform.cpp)

add_library(hyprendfield_account_core STATIC
    src/endfield/modules/hypergryph_account_model.cpp
    src/endfield/modules/hypergryph_account_crypto.cpp
    src/endfield/modules/hypergryph_account_unicode.cpp
    src/endfield/modules/hypergryph_account_api.cpp
    src/endfield/modules/hypergryph_account_cache.cpp
    src/endfield/modules/hypergryph_account_controller.cpp
    src/endfield/modules/hypergryph_account_strings.cpp
    src/endfield/modules/hypergryph_account_sanity_gauge.cpp
    src/endfield/modules/profile_artwork.cpp
    src/endfield/modules/hypergryph_account_login.cpp)
target_include_directories(hyprendfield_account_core PUBLIC src/endfield)
target_link_libraries(hyprendfield_account_core PUBLIC Qt6::Core)

qt_add_executable(hyprendfield
    src/main.cpp
    src/applicationcontroller.cpp
    src/applicationcontroller.hpp
    src/settingscontroller.cpp
    src/settingscontroller.hpp
    src/centerlogo.cpp
    src/centerlogo.hpp
    src/facilitycontroller.cpp
    src/facilitycontroller.hpp
    src/storagecontroller.cpp
    src/storagecontroller.hpp
    src/profilecontroller.cpp
    src/profilecontroller.hpp
    src/profileimageitem.cpp src/profileimageitem.hpp
    src/accountcontroller.cpp src/accountcontroller.hpp
    src/accountlogin.cpp src/accountlogin.hpp
    src/accountartwork.cpp src/accountartwork.hpp
    src/orbipomcontroller.cpp
    src/orbipomcontroller.hpp
    src/appshortcutscontroller.cpp src/appshortcutscontroller.hpp
    ${EVENT_LOG_SOURCES}
    src/mediainfo.cpp
    src/mediainfo.hpp
    src/projectioncanvas.cpp
    src/projectioncanvas.hpp
    src/mediaeditor.cpp
    src/mediaeditor.hpp
    src/mediaengine.cpp
    src/mediaengine.hpp
    src/endfield/modules/media_assembly_model.cpp
    src/endfield/modules/media_assembly_geometry.cpp
    src/endfield/modules/media_assembly_processor.cpp
    src/watchanimationcontroller.cpp
    src/watchanimationcontroller.hpp
    src/sourcedial.cpp
    src/sourcedial.hpp
    src/batterycontroller.cpp
    src/batterycontroller.hpp
    src/nowplayingcontroller.cpp
    src/nowplayingcontroller.hpp
    src/splayerlyrics.cpp
    src/splayerlyrics.hpp
    src/audiocontroller.cpp
    src/audiocontroller.hpp
    src/terminalsession.cpp
    src/terminalsession.hpp
    src/terminalview.cpp
    src/terminalview.hpp
    src/activitycollector.cpp
    src/activitycollector.hpp
    src/activitycontroller.cpp
    src/activitycontroller.hpp
    src/calendarcontroller.cpp
    src/calendarcontroller.hpp
    src/focusmodecontroller.cpp
    src/focusmodecontroller.hpp
    src/clipboardcontroller.cpp
    src/clipboardcontroller.hpp
    src/notescontroller.cpp
    src/notescontroller.hpp
    src/fileshelfcontroller.cpp
    src/fileshelfcontroller.hpp
    src/archivecontroller.cpp
    src/archivecontroller.hpp
    src/readercontroller.cpp
    src/readercontroller.hpp
    ${WLR_DATA_CONTROL_HEADER}
    ${WLR_DATA_CONTROL_SOURCE}
    ${EXT_DATA_CONTROL_HEADER}
    ${EXT_DATA_CONTROL_SOURCE}
)

target_include_directories(hyprendfield PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" src/endfield)

set_source_files_properties(qml/Appearance.js qml/ModuleNumbering.js PROPERTIES QT_QML_SKIP_QMLDIR_ENTRY TRUE)
qt_add_qml_module(hyprendfield
    URI HyprEndfield
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ProfilePage.qml
        qml/AccountPage.qml
        qml/AccountGauge.qml
        qml/GameProfileBadge.qml
        qml/ProfileCardSource.js
        qml/ProfileField.qml
        qml/OrbiPomPage.qml
        qml/EventLogPage.qml
        qml/AppShortcutsPage.qml
        qml/WatchShell.qml
        qml/SystemPage.qml
        qml/DisplayPage.qml
        qml/AboutPage.qml
        qml/SettingsHeader.qml
        qml/SettingsMenu.qml
        qml/DisplayExtras.qml
        qml/ClockInstrument.qml
        qml/BatteryAlertWindow.qml
        qml/Appearance.js
        qml/ModuleNumbering.js
        qml/Redaction.qml
        qml/RedactionBlock.qml
        qml/BootLog.qml
        qml/FacilityBanner.qml
        qml/FacilityTags.qml
        qml/HazardStripes.qml
        qml/NowPlayingPage.qml
        qml/NowPlayingButton.qml
        qml/NowPlayingSection.qml
        qml/VolumePage.qml
        qml/VolumeRow.qml
        qml/VolumeSlider.qml
        qml/VolumeDropdown.qml
        qml/TerminalPage.qml
        qml/ActivityPage.qml
        qml/StoragePage.qml
        qml/ProjectionPage.qml
        qml/ProjectionTools.qml
        qml/ProjectionWorkspace.qml
        qml/MediaAssemblyPage.qml
        qml/SourceGlyph.qml
        qml/ActivityMetricRow.qml
        qml/ActivityFormat.js
        qml/CalendarPage.qml
        qml/CalendarFormat.js
        qml/CalendarButton.qml
        qml/CalendarDayCell.qml
        qml/CalendarTextField.qml
        qml/CalendarEventPlate.qml
        qml/CalendarMonthPicker.qml
        qml/CalendarConfirmBar.qml
        qml/FocusModePage.qml
        qml/FocusModeButton.qml
        qml/ClipboardPage.qml
        qml/ClipboardCard.qml
        qml/ClipboardGlyph.qml
        qml/ClipboardConfirmBar.qml
        qml/NotesPage.qml
        qml/NotesWorkspace.qml
        qml/NoteCard.qml
        qml/MediaPreview.qml
        qml/MediaSourceMenu.qml
        qml/NoteFormatMenu.qml
        qml/ColorWheel.qml
        qml/NotesTaskCard.qml
        qml/NotesFormatBar.qml
        qml/NotesAddButton.qml
        qml/NotesGlyph.qml
        qml/FileShelfPage.qml
        qml/ShelfTile.qml
        qml/ShelfDocument.qml
        qml/ShelfProperties.qml
        qml/ArchivePage.qml
        qml/ArchiveCategoryPlate.qml
        qml/ArchiveTile.qml
        qml/ArchiveMediaStrip.qml
        qml/ArchiveLockedPanel.qml
        qml/ReaderPage.qml
        qml/ReaderTabs.qml
        qml/ReaderGlyph.qml
        qml/ReaderBookTile.qml
        qml/ReaderPageSlider.qml
        qml/HudSettingsRow.qml
        qml/HudSlider.qml
        qml/HudSounds.qml
        qml/HudGlyph.qml
        qml/MenuArt.qml
        qml/MenuPlane.qml
        qml/MenuCard.qml
        qml/MenuTile.qml
        qml/MenuClock.qml
        qml/MenuBadge.qml
        qml/MenuScene.qml
        qml/MenuGeometry.js
        qml/WatchSurfaces.js
        qml/SourceSurface.qml
        qml/SourceBottomButton.qml
        qml/BatteryBadge.qml
)

execute_process(COMMAND git rev-parse --short=8 HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE HYPR_BUILD_ID OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT HYPR_BUILD_ID)
    set(HYPR_BUILD_ID "source")
endif()
target_compile_definitions(hyprendfield PRIVATE HYPR_VERSION="${PROJECT_VERSION}" HYPR_BUILD_ID="${HYPR_BUILD_ID}")
file(GLOB_RECURSE SETTINGS_ASSETS CONFIGURE_DEPENDS "assets/settings/*")
# Predecoded PCM keeps short feedback sounds ready without decoding on every input.
# Originals remain in assets/audio; encode derivatives with ffmpeg -i <mp3> -map_metadata -1 -c:a pcm_s16le <wav>.
set(HUD_SOUND_ASSETS
    assets/audio/HyprField_Open.wav
    assets/audio/HyprField_Close.wav
    assets/audio/HyprField_Select.wav
    assets/audio/HyprField_Click.wav)
qt_add_resources(hyprendfield "hud_sound_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HUD_SOUND_ASSETS})
file(GLOB_RECURSE MEDIA_ASSETS CONFIGURE_DEPENDS "assets/media-assembly/*")
file(GLOB_RECURSE ORBIPOM_ASSETS CONFIGURE_DEPENDS "assets/orbipom/*")
file(GLOB_RECURSE SHORTCUT_ASSETS CONFIGURE_DEPENDS "assets/app-shortcut/*")
file(GLOB_RECURSE ACCOUNT_ASSETS CONFIGURE_DEPENDS "assets/account/*" "assets/profile-card/*")
qt_add_resources(hyprendfield "account_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ACCOUNT_ASSETS})
qt_add_resources(hyprendfield "shortcut_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SHORTCUT_ASSETS})
qt_add_resources(hyprendfield "orbipom_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ORBIPOM_ASSETS})
qt_add_resources(hyprendfield "media_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${MEDIA_ASSETS})
qt_add_resources(hyprendfield "settings_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SETTINGS_ASSETS})
file(GLOB MENU_VECTOR_ASSETS CONFIGURE_DEPENDS "assets/menu/*.svg" "assets/menu/*.png")
qt_add_resources(hyprendfield "menu_vectors"
    PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${MENU_VECTOR_ASSETS})
qt_add_shaders(hyprendfield "menu_shaders" PREFIX "/"
    FILES shaders/viewport-mask.frag shaders/source-surface.frag)
file(GLOB WATCH_SURFACE_ASSETS CONFIGURE_DEPENDS "assets/watch-surfaces/*.png")
qt_add_resources(hyprendfield "watch_surfaces"
    PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${WATCH_SURFACE_ASSETS})
file(GLOB WATCH_DIAL_ASSETS CONFIGURE_DEPENDS "assets/watch-dial/*")
qt_add_resources(hyprendfield "watch_dial"
    PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${WATCH_DIAL_ASSETS}
        shaders/dial.vert shaders/dial.frag shaders/dial-composite.vert shaders/dial-composite.frag)

qt_add_resources(hyprendfield "hyprendfield_assets"
    PREFIX "/"
    FILES
        assets/watch/ui_main_loop_bg.png
        assets/watch/ui_out_ring.png
        assets/watch/ui_out1_ring.png
        assets/watch/ui_mid_ring.png
        assets/watch/ui_triangle_fx.png
        assets/watch/deco_04.png
        assets/watch/endfield_industries.png
        assets/watch/clips.json
        assets/watch/watch-blur.json
        assets/watch/runtime-root-camera.json
        assets/watch/scene.json
        assets/watch/SOURCES.json
        assets/icons/notes.png
        assets/icons/profile-frame.bgra
        assets/icons/projection.png
        assets/icons/minigame.png
        assets/icons/event-log.png
        assets/icons/media-assembly.png
        assets/icons/file-shelf.png
        assets/icons/clipboard.png
        assets/icons/archive.png
        assets/icons/reader.png
        assets/icons/work-mode.png
        assets/icons/calendar.png
        assets/icons/SOURCES.json
        assets/fonts/HarmonyOS_Sans_Regular.ttf
        assets/fonts/HarmonyOS_Sans_Medium.ttf
        assets/fonts/HarmonyOS_Sans_Bold.ttf
        assets/fonts/HarmonyOS-Sans-LICENSE.txt
)

target_link_libraries(hyprendfield PRIVATE
    hyprendfield_account_core
    PkgConfig::GIO
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::Network
    Qt6::DBus
    Qt6::WebSockets
    Qt6::Qml
    Qt6::Quick
    Qt6::OpenGL
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Multimedia
    Qt6::Pdf
    LayerShellQt::Interface
    KF6::GlobalAccel
    PkgConfig::PULSE
    PkgConfig::VTERM
    PkgConfig::WAYLAND_CLIENT
    PkgConfig::LIBSECRET
    PkgConfig::PAM
    PkgConfig::LIBZIP
    util
)

target_compile_options(hyprendfield PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

set_target_properties(hyprendfield PROPERTIES
    OUTPUT_NAME HyprEndfield
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(GNUInstallDirs)
option(BUILD_TESTING "Build local-only HyprEndfield tests" OFF)
include(CTest)
if(BUILD_TESTING AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/settings.cpp")
    message(FATAL_ERROR "BUILD_TESTING requires the local tests/ directory, which is not distributed. Configure with -DBUILD_TESTING=OFF.")
endif()
if(BUILD_TESTING)
    qt_add_executable(hyprendfield_settings_tests tests/settings.cpp
        src/settingscontroller.cpp src/settingscontroller.hpp src/centerlogo.cpp src/centerlogo.hpp
        src/facilitycontroller.cpp src/facilitycontroller.hpp src/applicationcontroller.cpp src/applicationcontroller.hpp)
    target_include_directories(hyprendfield_settings_tests PRIVATE src)
    target_link_libraries(hyprendfield_settings_tests PRIVATE Qt6::Quick Qt6::Test Qt6::Network Qt6::Concurrent Qt6::Multimedia LayerShellQt::Interface)
    target_compile_definitions(hyprendfield_settings_tests PRIVATE HYPR_VERSION="${PROJECT_VERSION}" HYPR_BUILD_ID="test"
        SETTINGS_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml" SETTINGS_PREVIEW_DIR="${CMAKE_CURRENT_BINARY_DIR}/settings-preview")
    qt_add_resources(hyprendfield_settings_tests "settings_test_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SETTINGS_ASSETS} ${HUD_SOUND_ASSETS} ${MENU_VECTOR_ASSETS}
        assets/icons/notes.png assets/icons/file-shelf.png assets/icons/clipboard.png assets/icons/archive.png
        assets/icons/reader.png assets/icons/work-mode.png assets/icons/calendar.png assets/icons/projection.png assets/icons/media-assembly.png assets/icons/minigame.png assets/icons/event-log.png)
    add_test(NAME settings COMMAND hyprendfield_settings_tests)
    set_tests_properties(settings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 45)
endif()
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
    qt_add_executable(hyprendfield_player_tests tests/now_playing.cpp
        src/nowplayingcontroller.cpp src/nowplayingcontroller.hpp
        src/splayerlyrics.cpp src/splayerlyrics.hpp)
    target_include_directories(hyprendfield_player_tests PRIVATE src)
    target_link_libraries(hyprendfield_player_tests PRIVATE Qt6::Core Qt6::DBus Qt6::WebSockets Qt6::Test)
    add_test(NAME now_playing COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:hyprendfield_player_tests>)
    qt_add_executable(hyprendfield_player_page_tests tests/player_page.cpp
        src/nowplayingcontroller.cpp src/nowplayingcontroller.hpp
        src/splayerlyrics.cpp src/splayerlyrics.hpp)
    target_include_directories(hyprendfield_player_page_tests PRIVATE src)
    target_compile_definitions(hyprendfield_player_page_tests PRIVATE
        PLAYER_PAGE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/NowPlayingPage.qml")
    target_link_libraries(hyprendfield_player_page_tests PRIVATE Qt6::Quick Qt6::DBus Qt6::WebSockets)
    add_test(NAME player_page COMMAND hyprendfield_player_page_tests)
    set_tests_properties(player_page PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(hyprendfield_visual_tests
        tests/scene_projection.cpp src/watchanimationcontroller.cpp src/watchanimationcontroller.hpp)
    target_include_directories(hyprendfield_visual_tests PRIVATE src)
    target_link_libraries(hyprendfield_visual_tests PRIVATE Qt6::Core Qt6::Gui)
    qt_add_resources(hyprendfield_visual_tests "test_watch_data" PREFIX "/" FILES
        assets/watch/clips.json assets/watch/watch-blur.json
        assets/watch/runtime-root-camera.json assets/watch/scene.json assets/profile-card/scene.json)
    add_test(NAME scene_projection COMMAND hyprendfield_visual_tests)
    qt_add_executable(hyprendfield_battery_tests tests/battery.cpp src/batterycontroller.cpp src/batterycontroller.hpp)
    target_include_directories(hyprendfield_battery_tests PRIVATE src)
    target_link_libraries(hyprendfield_battery_tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME battery COMMAND hyprendfield_battery_tests)
    qt_add_executable(hyprendfield_battery_page_tests tests/battery_page.cpp src/batterycontroller.cpp src/batterycontroller.hpp)
    target_include_directories(hyprendfield_battery_page_tests PRIVATE src)
    target_link_libraries(hyprendfield_battery_page_tests PRIVATE Qt6::Quick Qt6::Test)
    target_compile_definitions(hyprendfield_battery_page_tests PRIVATE BATTERY_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/BatteryBadge.qml")
    add_test(NAME battery_page COMMAND hyprendfield_battery_page_tests)
    set_tests_properties(battery_page PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    qt_add_executable(hyprendfield_audio_tests tests/audio.cpp src/audiocontroller.cpp src/audiocontroller.hpp)
    target_include_directories(hyprendfield_audio_tests PRIVATE src)
    target_link_libraries(hyprendfield_audio_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::PULSE)
    add_test(NAME audio COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:hyprendfield_audio_tests>)
    set_tests_properties(audio PROPERTIES TIMEOUT 60)
    qt_add_executable(hyprendfield_volume_page_tests tests/volume_page.cpp src/audiocontroller.cpp src/audiocontroller.hpp)
    target_include_directories(hyprendfield_volume_page_tests PRIVATE src)
    target_compile_definitions(hyprendfield_volume_page_tests PRIVATE VOLUME_PAGE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/VolumePage.qml")
    target_link_libraries(hyprendfield_volume_page_tests PRIVATE Qt6::Quick Qt6::Test PkgConfig::PULSE)
    add_test(NAME volume_page COMMAND hyprendfield_volume_page_tests)
    set_tests_properties(volume_page PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(hyprendfield_terminal_tests tests/terminal.cpp
        src/terminalsession.cpp src/terminalsession.hpp src/terminalview.cpp src/terminalview.hpp)
    target_include_directories(hyprendfield_terminal_tests PRIVATE src)
    target_link_libraries(hyprendfield_terminal_tests PRIVATE Qt6::Quick Qt6::Test PkgConfig::VTERM util)
    target_compile_definitions(hyprendfield_terminal_tests PRIVATE TERMINAL_PAGE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/TerminalPage.qml")
    add_test(NAME terminal COMMAND hyprendfield_terminal_tests)
    set_tests_properties(terminal PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
    qt_add_executable(hyprendfield_activity_tests tests/activity.cpp
        src/activitycollector.cpp src/activitycollector.hpp src/activitycontroller.cpp src/activitycontroller.hpp)
    target_include_directories(hyprendfield_activity_tests PRIVATE src)
    target_link_libraries(hyprendfield_activity_tests PRIVATE Qt6::Quick Qt6::Concurrent Qt6::Test)
    target_compile_definitions(hyprendfield_activity_tests PRIVATE ACTIVITY_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    add_test(NAME activity COMMAND hyprendfield_activity_tests)
    set_tests_properties(activity PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
    qt_add_executable(hyprendfield_calendar_tests tests/calendar.cpp src/calendarcontroller.cpp src/calendarcontroller.hpp)
    target_include_directories(hyprendfield_calendar_tests PRIVATE src)
    target_link_libraries(hyprendfield_calendar_tests PRIVATE Qt6::Quick Qt6::DBus Qt6::Test)
    target_compile_definitions(hyprendfield_calendar_tests PRIVATE CALENDAR_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    add_test(NAME calendar COMMAND hyprendfield_calendar_tests)
    set_tests_properties(calendar PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
    qt_add_executable(hyprendfield_focus_mode_tests tests/focus_mode.cpp
        src/focusmodecontroller.cpp src/focusmodecontroller.hpp)
    target_include_directories(hyprendfield_focus_mode_tests PRIVATE src)
    target_link_libraries(hyprendfield_focus_mode_tests PRIVATE Qt6::Quick Qt6::Test)
    target_compile_definitions(hyprendfield_focus_mode_tests PRIVATE
        FOCUS_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
        FOCUS_PREVIEW_PATH="${CMAKE_BINARY_DIR}/focus-mode-preview.png")
    add_test(NAME focus_mode COMMAND hyprendfield_focus_mode_tests)
    set_tests_properties(focus_mode PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
    qt_add_executable(hyprendfield_clipboard_tests tests/clipboard.cpp tests/clipboard_wayland_server.hpp
        src/clipboardcontroller.cpp src/clipboardcontroller.hpp
        ${WLR_DATA_CONTROL_HEADER} ${WLR_DATA_CONTROL_SOURCE} ${WLR_DATA_CONTROL_SERVER_HEADER}
        ${EXT_DATA_CONTROL_HEADER} ${EXT_DATA_CONTROL_SOURCE} ${EXT_DATA_CONTROL_SERVER_HEADER})
    target_include_directories(hyprendfield_clipboard_tests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(hyprendfield_clipboard_tests PRIVATE Qt6::Quick Qt6::Test PkgConfig::WAYLAND_CLIENT PkgConfig::WAYLAND_SERVER)
    target_compile_definitions(hyprendfield_clipboard_tests PRIVATE CLIPBOARD_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    add_test(NAME clipboard COMMAND hyprendfield_clipboard_tests)
    set_tests_properties(clipboard PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
    qt_add_executable(hyprendfield_notes_tests tests/notes.cpp
        src/notescontroller.cpp src/notescontroller.hpp)
    target_include_directories(hyprendfield_notes_tests PRIVATE src)
    target_link_libraries(hyprendfield_notes_tests PRIVATE Qt6::Quick Qt6::Concurrent Qt6::Test)
    target_compile_definitions(hyprendfield_notes_tests PRIVATE NOTES_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    add_test(NAME notes COMMAND hyprendfield_notes_tests)
    set_tests_properties(notes PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
    qt_add_executable(hyprendfield_shelf_tests tests/file_shelf.cpp
        src/fileshelfcontroller.cpp src/fileshelfcontroller.hpp
        src/settingscontroller.cpp src/settingscontroller.hpp
        src/applicationcontroller.cpp src/applicationcontroller.hpp)
    target_include_directories(hyprendfield_shelf_tests PRIVATE src)
    target_link_libraries(hyprendfield_shelf_tests PRIVATE Qt6::Quick Qt6::DBus Qt6::Concurrent Qt6::Test Qt6::Network LayerShellQt::Interface)
    target_compile_definitions(hyprendfield_shelf_tests PRIVATE SHELF_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml" HYPR_VERSION="${PROJECT_VERSION}" HYPR_BUILD_ID="test")
    add_test(NAME file_shelf COMMAND hyprendfield_shelf_tests)
    set_tests_properties(file_shelf PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
    qt_add_executable(hyprendfield_archive_tests tests/archive.cpp
        src/archivecontroller.cpp src/archivecontroller.hpp
        src/fileshelfcontroller.cpp src/fileshelfcontroller.hpp)
    target_include_directories(hyprendfield_archive_tests PRIVATE src)
    target_link_libraries(hyprendfield_archive_tests PRIVATE
        Qt6::Quick Qt6::Network Qt6::DBus Qt6::Concurrent Qt6::Sql Qt6::Multimedia Qt6::Test
        PkgConfig::LIBSECRET PkgConfig::PAM)
    target_compile_definitions(hyprendfield_archive_tests PRIVATE ARCHIVE_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    add_test(NAME archive COMMAND hyprendfield_archive_tests)
    set_tests_properties(archive PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    qt_add_executable(hyprendfield_reader_tests tests/reader.cpp
        src/readercontroller.cpp src/readercontroller.hpp
        src/fileshelfcontroller.cpp src/fileshelfcontroller.hpp)
    target_include_directories(hyprendfield_reader_tests PRIVATE src)
    target_link_libraries(hyprendfield_reader_tests PRIVATE
        Qt6::Quick Qt6::Concurrent Qt6::Pdf Qt6::DBus Qt6::Test PkgConfig::LIBZIP)
    target_compile_definitions(hyprendfield_reader_tests PRIVATE
        READER_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
        READER_PREVIEW_PATH="${CMAKE_BINARY_DIR}/reader-preview.png"
        READER_READING_PREVIEW_PATH="${CMAKE_BINARY_DIR}/reader-reading-preview.png"
        READER_ZOOM_PREVIEW_PATH="${CMAKE_BINARY_DIR}/reader-zoom-preview.png")
    add_test(NAME reader COMMAND hyprendfield_reader_tests)
    set_tests_properties(reader PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    qt_add_executable(hyprendfield_orbipom_tests tests/orbipom.cpp src/orbipomcontroller.cpp)
    target_include_directories(hyprendfield_orbipom_tests PRIVATE src)
    target_link_libraries(hyprendfield_orbipom_tests PRIVATE Qt6::Quick Qt6::Test)
    target_compile_definitions(hyprendfield_orbipom_tests PRIVATE ORBIPOM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" ORBIPOM_PREVIEW_DIR="${CMAKE_BINARY_DIR}")
    qt_add_resources(hyprendfield_orbipom_tests "orbipom_test_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ORBIPOM_ASSETS})
    add_test(NAME orbipom COMMAND hyprendfield_orbipom_tests)
    qt_add_executable(hyprendfield_event_log_tests tests/event_log.cpp ${EVENT_LOG_SOURCES})
    target_include_directories(hyprendfield_event_log_tests PRIVATE src src/endfield)
    target_link_libraries(hyprendfield_event_log_tests PRIVATE Qt6::Quick Qt6::Concurrent Qt6::Test)
    target_compile_definitions(hyprendfield_event_log_tests PRIVATE EVENT_LOG_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" EVENT_LOG_PREVIEW_DIR="${CMAKE_BINARY_DIR}")
    add_test(NAME event_log COMMAND hyprendfield_event_log_tests)
    qt_add_executable(hyprendfield_shortcut_tests tests/app_shortcuts.cpp src/appshortcutscontroller.cpp)
    target_include_directories(hyprendfield_shortcut_tests PRIVATE src)
    target_link_libraries(hyprendfield_shortcut_tests PRIVATE Qt6::Quick Qt6::Widgets Qt6::Test PkgConfig::GIO)
    target_compile_definitions(hyprendfield_shortcut_tests PRIVATE SHORTCUT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" SHORTCUT_PREVIEW_DIR="${CMAKE_BINARY_DIR}")
    qt_add_resources(hyprendfield_shortcut_tests "shortcut_test_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SHORTCUT_ASSETS})
    add_test(NAME app_shortcuts COMMAND hyprendfield_shortcut_tests)
    set_tests_properties(app_shortcuts PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 60)
    set_tests_properties(event_log PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 60)
    set_tests_properties(orbipom PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 60)
    qt_add_executable(hyprendfield_media_tests tests/media_components.cpp
        src/projectioncanvas.cpp src/mediainfo.cpp src/storagecontroller.cpp src/notescontroller.cpp src/profilecontroller.cpp src/profileimageitem.cpp
        src/mediaeditor.cpp src/mediaengine.cpp
        src/endfield/modules/media_assembly_model.cpp src/endfield/modules/media_assembly_geometry.cpp src/endfield/modules/media_assembly_processor.cpp)
    target_include_directories(hyprendfield_media_tests PRIVATE src src/endfield)
    target_link_libraries(hyprendfield_media_tests PRIVATE Qt6::Quick Qt6::Concurrent Qt6::Multimedia Qt6::Test)
    target_compile_definitions(hyprendfield_media_tests PRIVATE MEDIA_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml" MEDIA_PREVIEW_DIR="${CMAKE_BINARY_DIR}")
    qt_add_resources(hyprendfield_media_tests "media_test_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${MEDIA_ASSETS} assets/icons/profile-frame.bgra)
    add_test(NAME media_components COMMAND hyprendfield_media_tests)
    set_tests_properties(media_components PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 90)
    add_executable(hyprendfield_account_api_tests tests/account_api.cpp
        src/endfield/core/data/json.cpp src/endfield/core/data/file_io.cpp src/endfield/data_platform.cpp)
    target_link_libraries(hyprendfield_account_api_tests PRIVATE hyprendfield_account_core)
    add_test(NAME account_api COMMAND hyprendfield_account_api_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/hypergryph-account-source.json")
    qt_add_executable(hyprendfield_account_tests tests/account_components.cpp
        src/accountcontroller.cpp src/accountlogin.cpp src/accountartwork.cpp src/profilecontroller.cpp src/profileimageitem.cpp src/watchanimationcontroller.cpp
        src/endfield/core/data/json.cpp src/endfield/core/data/file_io.cpp src/endfield/data_platform.cpp)
    target_include_directories(hyprendfield_account_tests PRIVATE src)
    target_link_libraries(hyprendfield_account_tests PRIVATE hyprendfield_account_core Qt6::Quick Qt6::Test Qt6::WebEngineWidgets Qt6::Concurrent PkgConfig::LIBSECRET)
    target_compile_definitions(hyprendfield_account_tests PRIVATE ACCOUNT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" ACCOUNT_PREVIEW_DIR="${CMAKE_BINARY_DIR}")
    qt_add_resources(hyprendfield_account_tests "account_test_assets" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ACCOUNT_ASSETS}
        assets/icons/profile-frame.bgra assets/watch/clips.json assets/watch/watch-blur.json assets/watch/runtime-root-camera.json assets/watch/scene.json)
    add_test(NAME account_components COMMAND hyprendfield_account_tests)
    set_tests_properties(account_components PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 90)
endif()

install(TARGETS hyprendfield
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
