qt_add_executable(tst_project tst_project.cpp)
target_link_libraries(tst_project PRIVATE framepad_core Qt6::Test)
add_test(NAME tst_project COMMAND tst_project)
# The tests never show a window; run them without a display.
set_tests_properties(tst_project PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
