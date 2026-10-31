include(GoogleTest)
add_executable(core_tests core_tests.cpp standards_tests.cpp project_settings_tests.cpp limit_states_tests.cpp recent_files_tests.cpp options_settings_tests.cpp combination_report_tests.cpp project_archive_tests.cpp undo_stack_tests.cpp units_tests.cpp project_file_tests.cpp structure_model_tests.cpp general_data_tests.cpp camera_tests.cpp selection_tests.cpp snap_tests.cpp)
target_compile_definitions(core_tests PRIVATE CC_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(core_tests PRIVATE cc_core miniz::miniz GTest::gtest GTest::gtest_main)
gtest_discover_tests(core_tests)


# GUI tests (Qt offscreen platform; nothing is displayed).
add_executable(workarea_tests workarea_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/WorkArea.cpp)
target_include_directories(workarea_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(workarea_tests PRIVATE Qt6::Widgets GTest::gtest)
gtest_discover_tests(workarea_tests)

add_executable(viewport3d_tests viewport3d_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp)
target_include_directories(viewport3d_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(viewport3d_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Test GTest::gtest)
gtest_discover_tests(viewport3d_tests)

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

add_executable(projecttype_tests projecttype_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/ProjectTypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(projecttype_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(projecttype_tests PRIVATE Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(projecttype_tests)

add_executable(standardsdialog_tests standardsdialog_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/StandardsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(standardsdialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(standardsdialog_tests PRIVATE cc_core Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(standardsdialog_tests)

add_executable(emptywizard_tests emptywizard_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/EmptyProjectWizard.cpp ${CMAKE_SOURCE_DIR}/src/app/LimitStatesDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/SituationsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/WizardSubDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/OptionsDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/OptionsLeafDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/StandardsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(emptywizard_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(emptywizard_tests PRIVATE cc_core Qt6::PrintSupport Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(emptywizard_tests)

add_executable(limitstates_dialog_tests limitstates_dialog_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/LimitStatesDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/SituationsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(limitstates_dialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(limitstates_dialog_tests PRIVATE cc_core Qt6::PrintSupport Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(limitstates_dialog_tests)

add_executable(filemanager_tests filemanager_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/FileManagerDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/FileOpsDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(filemanager_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(filemanager_tests PRIVATE cc_core Qt6::Widgets Qt6::Svg Qt6::Test GTest::gtest)
gtest_discover_tests(filemanager_tests)

add_executable(situations_dialog_tests situations_dialog_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/SituationsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(situations_dialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(situations_dialog_tests PRIVATE cc_core Qt6::PrintSupport Qt6::Widgets Qt6::Svg Qt6::Test GTest::gtest)
gtest_discover_tests(situations_dialog_tests)

add_executable(units_dialog_tests units_dialog_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/UnitsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(units_dialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(units_dialog_tests PRIVATE cc_core Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(units_dialog_tests)
