cmake_minimum_required(VERSION 3.21)

project(uSnip VERSION 0.1.0 LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Test)
qt_standard_project_setup()

# Identity passed to codesign after each build. Empty means the linker's ad-hoc signature,
# which changes on every build and makes macOS ask for permissions again (see README).
set(USNIP_CODESIGN_IDENTITY "" CACHE STRING "codesign identity, e.g. \"uSnip Local\"")

# Core: storage format and crypto. No UI, no Objective-C; covered by the unit tests.
add_library(usnip_core STATIC
    src/core/Crypto.cpp
    src/core/ExportFile.cpp
    src/core/JsonImport.cpp
    src/core/SecureBytes.cpp
    src/core/SnippetStore.cpp
)
target_include_directories(usnip_core PUBLIC src)
target_link_libraries(usnip_core PUBLIC Qt6::Core)
target_compile_options(usnip_core PRIVATE -Wall -Wextra -Wpedantic)

if(APPLE)
    qt_add_executable(uSnip MACOSX_BUNDLE
        src/app/main.cpp
        src/app/EditorWindow.cpp
        src/app/EditorWindow.h
        src/app/PasteController.cpp
        src/app/PasteController.h
        src/app/PopupWindow.cpp
        src/app/PopupWindow.h
        src/platform/Auth.h
        src/platform/Auth.mm
        src/platform/GlobalHotkey.h
        src/platform/GlobalHotkey.mm
        src/platform/Keychain.h
        src/platform/Keychain.mm
        src/platform/Pasteboard.h
        src/platform/Pasteboard.mm
        src/platform/Paste.h
        src/platform/Paste.mm
        src/platform/StatusItem.h
        src/platform/StatusItem.mm
    )
    set_target_properties(uSnip PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.noriyoshiabe.usnip
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
    target_compile_options(uSnip PRIVATE -Wall -Wextra
        $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_link_libraries(uSnip PRIVATE
        usnip_core
        Qt6::Widgets
        "-framework AppKit"
        "-framework ApplicationServices"
        "-framework Carbon"
        "-framework LocalAuthentication"
        "-framework Security"
    )

    if(USNIP_CODESIGN_IDENTITY)
        add_custom_command(TARGET uSnip POST_BUILD
            COMMAND codesign --force --sign "${USNIP_CODESIGN_IDENTITY}" "$<TARGET_BUNDLE_DIR:uSnip>"
            COMMENT "Signing uSnip.app with ${USNIP_CODESIGN_IDENTITY}"
            VERBATIM)
    endif()

    # Bundles Qt into a copy of the .app so Homebrew Qt upgrades cannot break it, then signs
    # again. A copy, because relinking a bundle that already holds Qt mixes two Qt builds and
    # the app aborts on launch.
    find_program(MACDEPLOYQT macdeployqt HINTS "${Qt6_DIR}/../../../bin")
    set(USNIP_DEPLOY_BUNDLE "${CMAKE_BINARY_DIR}/deploy/uSnip.app")
    add_custom_target(deploy
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${USNIP_DEPLOY_BUNDLE}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:uSnip>"
            "${USNIP_DEPLOY_BUNDLE}"
        COMMAND "${MACDEPLOYQT}" "${USNIP_DEPLOY_BUNDLE}"
        COMMAND codesign --force --deep
            --sign "$<IF:$<BOOL:${USNIP_CODESIGN_IDENTITY}>,${USNIP_CODESIGN_IDENTITY},->"
            "${USNIP_DEPLOY_BUNDLE}"
        DEPENDS uSnip
        COMMENT "Bundling Qt into uSnip.app"
        VERBATIM)
endif()

enable_testing()
add_subdirectory(tests)
