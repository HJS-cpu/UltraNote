cmake_minimum_required(VERSION 3.20)
project(UltraNote VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Unicode
add_definitions(-DUNICODE -D_UNICODE)

# Source files
set(SOURCES
    src/main.cpp
    src/Application.cpp
    src/NoteWindow.cpp
    src/NoteListWindow.cpp
    src/Storage.cpp
    src/Localization.cpp
    src/SettingsDialog.cpp
    src/FindInNoteDialog.cpp
    src/AlarmScheduler.cpp
    src/AlarmPopupWindow.cpp
    src/AlarmConfigDialog.cpp
)

# Resource file
set(RESOURCES
    res/UltraNote.rc
)

add_executable(UltraNote WIN32 ${SOURCES} ${RESOURCES})

target_include_directories(UltraNote PRIVATE src res)

target_link_libraries(UltraNote PRIVATE
    user32
    shell32
    gdi32
    comctl32
    comdlg32
    uxtheme
    shlwapi
    winmm
)

# MSVC specific settings
if(MSVC)
    target_compile_options(UltraNote PRIVATE /W4 /utf-8)
    # Release: optimize for size, enable function-level linking
    target_compile_options(UltraNote PRIVATE
        $<$<CONFIG:Release>:/O1 /Gy /GS->)
    # Release: strip unreferenced code, fold identical functions
    target_link_options(UltraNote PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>)
    # Static link CRT for portability
    set_property(TARGET UltraNote PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # Embed application manifest for ComCtl v6 and DPI awareness
    target_sources(UltraNote PRIVATE res/UltraNote.manifest)
    set_property(SOURCE res/UltraNote.manifest PROPERTY VS_TOOL_OVERRIDE "Manifest")
endif()

# Copy lang/ directory next to the EXE after build
add_custom_command(TARGET UltraNote POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/lang"
        "$<TARGET_FILE_DIR:UltraNote>/lang"
    COMMENT "Copying language files..."
)
