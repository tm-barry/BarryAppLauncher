cmake_minimum_required(VERSION 3.16)

project(BarryAppLauncher VERSION 0.8.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Core QuickControls2 QuickDialogs2 Gui)

qt_standard_project_setup(REQUIRES 6.8)

include(FetchContent)

# zlib
# Force position independent code for all static libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
)
FetchContent_MakeAvailable(zlib)

set(ZLIB_LIBRARY ${zlib_BINARY_DIR}/libz.a CACHE FILEPATH "" FORCE)
set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE PATH "" FORCE)
set(ENABLE_ZLIB ON CACHE BOOL TRUE FORCE)

# libarchive
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_TEST OFF CACHE BOOL "" FORCE)

# libarchive
FetchContent_Declare(
  libarchive
  GIT_REPOSITORY https://github.com/libarchive/libarchive.git
  GIT_TAG v3.8.5
)
FetchContent_MakeAvailable(libarchive)

qt_add_resources(RESOURCES
    resources.qrc
)

qt_add_executable(barryapplauncher
    main.cpp
    ${RESOURCES}
)

qt_add_qml_module(barryapplauncher
    URI BarryAppLauncher
    VERSION 1.0
    QML_FILES
        qml/utils/FormatterUtil.qml
        qml/About.qml
        qml/AppInfo.qml
        qml/AppList.qml
        qml/ColorButton.qml
        qml/CopyButton.qml
        qml/IconButton.qml
        qml/Main.qml
        qml/Preferences.qml
        qml/RoundedGroupBox.qml
        qml/RoundedTextArea.qml
        qml/TransparentTextArea.qml
    SOURCES
        managers/appimagemanager.h
        managers/appimagemanager.cpp
        managers/clipboardmanager.h
        managers/clipboardmanager.cpp
        managers/errormanager.h
        managers/errormanager.cpp
        managers/settingsmanager.h
        managers/settingsmanager.cpp
        models/appimagemetadata.h
        models/appimagemetadata.cpp
        models/updaterfiltermodel.h
        models/updaterreleasemodel.h
        models/appimagemetadatalistmodel.h
        models/appimagemetadatalistmodel.cpp
        providers/memoryimageprovider.h
        providers/memoryimageprovider.cpp
        utils/updater/jsonupdater.h
        utils/updater/jsonupdater.cpp
        utils/updater/staticupdater.h
        utils/updater/staticupdater.cpp
        utils/updater/updaterfactory.h
        utils/updater/updaterfactory.cpp
        utils/appimageutil.h
        utils/appimageutil.cpp
        utils/archiveutil.h
        utils/archiveutil.cpp
        utils/jsonutil.h
        utils/networkutil.h
        utils/semverutil.h
        utils/stringutil.h
        utils/terminalutil.h
        utils/terminalutil.cpp
        utils/texteditorutil.h
        utils/texteditorutil.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(barryapplauncher PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.barryapplauncher
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(barryapplauncher
    PRIVATE Qt6::Quick Qt6::Core Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::Gui
            archive_static
)

include(GNUInstallDirs)
install(TARGETS barryapplauncher
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
