qt_add_executable(tst_project tst_project.cpp)
target_link_libraries(tst_project PRIVATE framepad_core Qt6::Test)
add_test(NAME tst_project COMMAND tst_project)

# Makes its sample clips with the ffmpeg.exe of the FFmpeg package, so no media is checked in.
qt_add_executable(tst_media tst_media.cpp)
target_link_libraries(tst_media PRIVATE framepad_core Qt6::Test)
target_compile_definitions(tst_media PRIVATE FRAMEPAD_TEST_FFMPEG="${FFMPEG_ROOT}/bin/ffmpeg.exe")
add_test(NAME tst_media COMMAND tst_media)
# Windows looks for DLLs next to the .exe first; do not rely on PATH.
file(GLOB FFMPEG_DLLS "${FFMPEG_ROOT}/bin/*.dll")
add_custom_command(TARGET tst_media POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FFMPEG_DLLS} $<TARGET_FILE_DIR:tst_media>)

# Console programs, so QTest's report reaches ctest (Qt's project defaults make Windows GUI executables).
set_target_properties(tst_project tst_media PROPERTIES WIN32_EXECUTABLE FALSE)

# The tests never show a window; run them without a display.
set_tests_properties(tst_project tst_media PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
