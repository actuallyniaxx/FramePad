cmake_minimum_required(VERSION 3.21)

project(FramePad VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets LinguistTools Test)
qt_standard_project_setup()

# Everything except main.cpp lives in a static library so the tests can link it.
add_library(framepad_core STATIC
    src/project/Project.h
    src/project/Project.cpp
    src/project/Commands.h
    src/project/Commands.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/TimelinePanel.h
    src/ui/TimelinePanel.cpp
    src/ui/InspectorPanel.h
    src/ui/InspectorPanel.cpp
    src/ui/Language.h
    src/ui/Language.cpp
)
target_include_directories(framepad_core PUBLIC src)
target_link_libraries(framepad_core PUBLIC Qt6::Widgets)
target_compile_definitions(framepad_core PUBLIC FRAMEPAD_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(framepad_core PUBLIC /W4 /permissive- /utf-8)
endif()

qt_add_executable(FramePad WIN32 src/main.cpp)
target_link_libraries(FramePad PRIVATE framepad_core)

# English is the source language. Each translation is a .ts file here; its .qm is
# embedded under :/i18n. To add a language, add its .ts and run the update_translations target.
qt_add_translations(FramePad
    TS_FILES translations/FramePad_es.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete
    SOURCE_TARGETS framepad_core FramePad
)

enable_testing()
add_subdirectory(tests)

install(TARGETS FramePad RUNTIME DESTINATION .)
