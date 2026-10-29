qt_add_executable(tst_project tst_project.cpp)
target_link_libraries(tst_project PRIVATE framepad_core Qt6::Test)
add_test(NAME tst_project COMMAND tst_project)

# Makes its sample clips with the ffmpeg.exe of the FFmpeg package, so no media is checked in.
qt_add_executable(tst_media tst_media.cpp)
target_link_libraries(tst_media PRIVATE framepad_core Qt6::Test)
target_compile_definitions(tst_media PRIVATE FRAMEPAD_TEST_FFMPEG="${FFMPEG_ROOT}/bin/ffmpeg.exe")
add_test(NAME tst_media COMMAND tst_media)

# The tests never show a window; run them without a display.
set_tests_properties(tst_project tst_media PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
