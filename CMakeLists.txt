cmake_minimum_required(VERSION 3.16)
project(TXDEdit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 (or Qt5)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# Core library
add_library(txd_core STATIC
    core/TXDTypes.h
    core/TXDTextureHeader.h
    core/TXDTextureHeader.cpp
    core/TXDArchive.h
    core/TXDArchive.cpp
    core/TXDConverter.h
    core/TXDConverter.cpp
)

target_include_directories(txd_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Squish library for DXT compression (libsquish 1.10 - local copy)
set(SQUISH_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish/squish.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish/alpha.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish/clusterfit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish/colourblock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish/colourfit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish/colourset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish/maths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish/rangefit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish/singlecolourfit.cpp
)

# Detect SSE support for squish
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
    # Enable SSE2 for x86/x64
    add_compile_definitions(SQUISH_USE_SSE=2)
endif()

add_library(squish STATIC ${SQUISH_SOURCES})
target_include_directories(squish PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsquish
)
target_link_libraries(txd_core PUBLIC squish)

# libimagequant library for palette generation (C library, local copy)
# Enable C language for libimagequant
enable_language(C)

set(LIBIMAGEQUANT_SOURCES
    vendor/libimagequant/libimagequant.c
    vendor/libimagequant/blur.c
    vendor/libimagequant/kmeans.c
    vendor/libimagequant/mediancut.c
    vendor/libimagequant/mempool.c
    vendor/libimagequant/nearest.c
    vendor/libimagequant/pam.c
    vendor/libimagequant/remap.c
)

add_library(libimagequant STATIC ${LIBIMAGEQUANT_SOURCES})
set_target_properties(libimagequant PROPERTIES
    LINKER_LANGUAGE C
)
target_include_directories(libimagequant PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libimagequant
)

target_link_libraries(txd_core PUBLIC squish libimagequant)


# GUI application
set(GUI_SOURCES
    gui/main.cpp
    gui/MainWindow.h
    gui/MainWindow.cpp
    gui/TexturePreviewWidget.h
    gui/TexturePreviewWidget.cpp
    gui/TexturePropertiesWidget.h
    gui/TexturePropertiesWidget.cpp
    gui/TextureListWidget.h
    gui/TextureListWidget.cpp
    gui/TextureViewWidget.h
    gui/TextureViewWidget.cpp
    gui/CheckBox.h
    gui/AboutDialog.h
    gui/AboutDialog.cpp
    gui/GameVersionDialog.h
    gui/GameVersionDialog.cpp
    resources.qrc
)

# Use Qt's automoc for MOC files
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    add_executable(txdedit WIN32 ${GUI_SOURCES})
elseif(APPLE)
    add_executable(txdedit MACOSX_BUNDLE ${GUI_SOURCES})
else()
    add_executable(txdedit ${GUI_SOURCES})
endif()

target_link_libraries(txdedit
    txd_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

# Installation
if(APPLE)
    install(TARGETS txdedit
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS txdedit
        RUNTIME DESTINATION bin
    )
endif()

# Set output directories
if(WIN32)
    set_target_properties(txdedit PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
    )
else()
    set_target_properties(txdedit PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# Set application icon
if(APPLE)
    # macOS: Set icon using Info.plist
    set(MACOSX_BUNDLE_ICON_FILE mac.icns)
    set_target_properties(txdedit PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE ${MACOSX_BUNDLE_ICON_FILE}
    )
    # Copy icon to bundle Resources
    set_source_files_properties(icons/mac.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(txdedit PRIVATE icons/mac.icns)
elseif(WIN32)
    # Windows: Embed icon resource
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/icons/windows.ico)
        # Create resource file for icon
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/app.rc
            "IDI_ICON1 ICON DISCARDABLE \"${CMAKE_CURRENT_SOURCE_DIR}/icons/windows.ico\"\n"
        )
        target_sources(txdedit PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
    endif()
endif()

# Copy icons to build directory
if(WIN32)
    # On Windows, copy to both bin and bin/Release for consistency
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/icons DESTINATION ${CMAKE_BINARY_DIR}/bin)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/icons DESTINATION ${CMAKE_BINARY_DIR}/bin/Release)
else()
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/icons DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()

# Copy logos to build directory
if(WIN32)
    # On Windows, copy to both bin and bin/Release for consistency
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/logos DESTINATION ${CMAKE_BINARY_DIR}/bin)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/logos DESTINATION ${CMAKE_BINARY_DIR}/bin/Release)
else()
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/logos DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()

if(WIN32)
    # Copy Qt DLLs on Windows
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
    include(InstallRequiredSystemLibraries)
endif()

# GoogleTest for unit testing
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Test executable
add_executable(txd_tests
    tests/test_TXDTextureHeader.cpp
    tests/test_TXDConverter.cpp
    tests/test_TXDArchive.cpp
)

target_link_libraries(txd_tests
    txd_core
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

# Add test directory to include path
target_include_directories(txd_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(txd_tests)

