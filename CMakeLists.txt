cmake_minimum_required(VERSION 3.21)

project(FramePad VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets Concurrent ShaderTools LinguistTools Test)
qt_standard_project_setup()

# FFmpeg: FFMPEG_ROOT points at a "shared" development package with include/, lib/ (MSVC .lib
# import libraries) and bin/ (DLLs), such as BtbN's win64-gpl-shared builds that CI downloads.
set(FFMPEG_ROOT "" CACHE PATH "FFmpeg development package (include/, lib/, bin/)")
file(TO_CMAKE_PATH "${FFMPEG_ROOT}" FFMPEG_ROOT) # forward slashes: it also ends up in a C string (tests)
find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h HINTS "${FFMPEG_ROOT}/include" REQUIRED)
add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
target_include_directories(FFmpeg::FFmpeg SYSTEM INTERFACE "${FFMPEG_INCLUDE_DIR}")
foreach(component avformat avcodec avutil swscale swresample)
    find_library(FFMPEG_${component}_LIBRARY ${component} HINTS "${FFMPEG_ROOT}/lib" REQUIRED)
    target_link_libraries(FFmpeg::FFmpeg INTERFACE "${FFMPEG_${component}_LIBRARY}")
endforeach()

# Everything except main.cpp lives in a static library so the tests can link it.
add_library(framepad_core STATIC
    src/project/Project.h
    src/project/Project.cpp
    src/project/Commands.h
    src/project/Commands.cpp
    src/media/MediaTypes.h
    src/media/FFmpeg.h
    src/media/MediaReader.h
    src/media/MediaReader.cpp
    src/media/Previews.h
    src/media/Previews.cpp
    src/media/Player.h
    src/media/Player.cpp
    src/audio/AudioOutput.h
    src/audio/AudioOutput_wasapi.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/TimelinePanel.h
    src/ui/TimelinePanel.cpp
    src/ui/InspectorPanel.h
    src/ui/InspectorPanel.cpp
    src/ui/Language.h
    src/ui/Language.cpp
    src/ui/MediaPanel.h
    src/ui/MediaPanel.cpp
    src/ui/ViewerPanel.h
    src/ui/ViewerPanel.cpp
    src/ui/VideoWidget.h
    src/ui/VideoWidget.cpp
)
target_include_directories(framepad_core PUBLIC src)
# GuiPrivate is what Qt asks for to use QRhi (the GPU layer under QRhiWidget).
target_link_libraries(framepad_core PUBLIC Qt6::Widgets Qt6::Concurrent Qt6::GuiPrivate FFmpeg::FFmpeg ole32)
target_compile_definitions(framepad_core PUBLIC FRAMEPAD_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(framepad_core PUBLIC /W4 /permissive- /utf-8)
endif()

qt_add_executable(FramePad WIN32 src/main.cpp)
target_link_libraries(FramePad PRIVATE framepad_core)

# Viewer shaders, compiled to .qsb at build time and embedded under :/shaders.
qt_add_shaders(FramePad "shaders"
    PREFIX "/"
    GLSL "300es,330"
    FILES shaders/yuv.vert shaders/yuv.frag
)

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
