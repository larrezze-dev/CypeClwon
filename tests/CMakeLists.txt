include(GoogleTest)
add_executable(core_tests core_tests.cpp)
target_link_libraries(core_tests PRIVATE cc_core GTest::gtest GTest::gtest_main)
gtest_discover_tests(core_tests)


# GUI tests (Qt offscreen platform; nothing is displayed).
add_executable(workarea_tests workarea_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/WorkArea.cpp)
target_include_directories(workarea_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(workarea_tests PRIVATE Qt6::Widgets GTest::gtest)
gtest_discover_tests(workarea_tests)

add_executable(startpage_tests startpage_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/StartPage.cpp
  ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(startpage_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(startpage_tests PRIVATE Qt6::Widgets Qt6::Svg Qt6::Test GTest::gtest)
gtest_discover_tests(startpage_tests)

add_executable(newproject_tests newproject_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/NewProjectDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp)
target_include_directories(newproject_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(newproject_tests PRIVATE Qt6::Widgets GTest::gtest)
gtest_discover_tests(newproject_tests)
