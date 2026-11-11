include(GoogleTest)
add_executable(core_tests core_tests.cpp standards_tests.cpp project_settings_tests.cpp limit_states_tests.cpp recent_files_tests.cpp options_settings_tests.cpp combination_report_tests.cpp project_archive_tests.cpp undo_stack_tests.cpp units_tests.cpp project_file_tests.cpp structure_model_tests.cpp general_data_tests.cpp camera_tests.cpp selection_tests.cpp snap_tests.cpp display_options_tests.cpp categories_tests.cpp clipping_tests.cpp work_planes_tests.cpp node_edit_tests.cpp bar_edit_tests.cpp piece_edit_tests.cpp shell_edit_tests.cpp tools_edit_tests.cpp generate_edit_tests.cpp reference_edit_tests.cpp import_tests.cpp continuous_edit_tests.cpp section_properties_tests.cpp profile_library_tests.cpp concrete_timber_tests.cpp bar_description_tests.cpp describir_catalogue_tests.cpp node_support_tests.cpp node_ligature_tests.cpp node_diaphragm_tests.cpp bar_ends_tests.cpp buckling_tests.cpp deflection_group_tests.cpp load_case_tests.cpp nodal_load_tests.cpp bar_load_tests.cpp shell_load_tests.cpp panel_tests.cpp shell_properties_tests.cpp combinations_tests.cpp user_situations_tests.cpp)
target_compile_definitions(core_tests PRIVATE CC_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(core_tests PRIVATE cc_core miniz::miniz GTest::gtest GTest::gtest_main)
gtest_discover_tests(core_tests)


# GUI tests (Qt offscreen platform; nothing is displayed).
add_executable(workarea_tests workarea_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/WorkArea.cpp)
target_include_directories(workarea_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(workarea_tests PRIVATE Qt6::Widgets GTest::gtest)
gtest_discover_tests(workarea_tests)

add_executable(viewport3d_tests viewport3d_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp)
target_include_directories(viewport3d_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(viewport3d_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Test GTest::gtest)
gtest_discover_tests(viewport3d_tests)

add_executable(viewbar_tests viewbar_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/ViewBar.cpp ${CMAKE_SOURCE_DIR}/src/app/ViewIcons.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/ViewOptionsDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(viewbar_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(viewbar_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Test GTest::gtest)
gtest_discover_tests(viewbar_tests)

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

add_executable(emptywizard_tests emptywizard_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/EmptyProjectWizard.cpp ${CMAKE_SOURCE_DIR}/src/app/LimitStatesDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/UserSituationsDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/SituationsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/WizardSubDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/LoadCaseDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/OptionsDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/OptionsLeafDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/StandardsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(emptywizard_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(emptywizard_tests PRIVATE cc_core Qt6::PrintSupport Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(emptywizard_tests)

add_executable(limitstates_dialog_tests limitstates_dialog_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/LimitStatesDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/UserSituationsDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/SituationsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(limitstates_dialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(limitstates_dialog_tests PRIVATE cc_core Qt6::PrintSupport Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(limitstates_dialog_tests)

add_executable(user_situations_dialog_tests user_situations_dialog_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/LimitStatesDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/UserSituationsDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/SituationsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/StandardsDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(user_situations_dialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(user_situations_dialog_tests PRIVATE cc_core Qt6::PrintSupport Qt6::Widgets Qt6::Svg Qt6::Test GTest::gtest)
gtest_discover_tests(user_situations_dialog_tests)

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

add_executable(leftpanel_tests leftpanel_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/LeftPanel.cpp ${CMAKE_SOURCE_DIR}/src/app/DisplayOptionsPanel.cpp ${CMAKE_SOURCE_DIR}/src/app/LayersPanel.cpp ${CMAKE_SOURCE_DIR}/src/app/CategoriesPanel.cpp
  ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp)
target_include_directories(leftpanel_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(leftpanel_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Test GTest::gtest)
gtest_discover_tests(leftpanel_tests)

add_executable(layers_panel_tests layers_panel_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/LayersPanel.cpp)
target_include_directories(layers_panel_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(layers_panel_tests PRIVATE cc_core Qt6::Widgets Qt6::Test GTest::gtest)
gtest_discover_tests(layers_panel_tests)

add_executable(work_planes_dialog_tests work_planes_dialog_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/WorkPlanesDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(work_planes_dialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(work_planes_dialog_tests PRIVATE cc_core Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(work_planes_dialog_tests)

add_executable(cypedialog_tests cypedialog_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp)
target_include_directories(cypedialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(cypedialog_tests PRIVATE Qt6::Widgets GTest::gtest)
gtest_discover_tests(cypedialog_tests)

add_executable(node_tool_tests node_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/NodeTool.cpp ${CMAKE_SOURCE_DIR}/src/app/NodeCalculatorDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp)
target_include_directories(node_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(node_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Test GTest::gtest)
gtest_discover_tests(node_tool_tests)

add_executable(bar_tool_tests bar_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/BarTool.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp)
target_include_directories(bar_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(bar_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Test GTest::gtest)
gtest_discover_tests(bar_tool_tests)

add_executable(piece_tool_tests piece_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/PieceTool.cpp ${CMAKE_SOURCE_DIR}/src/app/BarTool.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp)
target_include_directories(piece_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(piece_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Test GTest::gtest)
gtest_discover_tests(piece_tool_tests)

add_executable(profile_library_dialogs_tests profile_library_dialogs_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/ProfileLibraryDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(profile_library_dialogs_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(profile_library_dialogs_tests PRIVATE cc_core Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(profile_library_dialogs_tests)

add_executable(bar_describe_tests bar_describe_dialog_tests.cpp bar_describe_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/BarDescribeDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/DescribePictograms.cpp
  ${CMAKE_SOURCE_DIR}/src/app/BarDescribeTool.cpp ${CMAKE_SOURCE_DIR}/src/app/ProfileLibraryDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(bar_describe_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(bar_describe_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::Test GTest::gtest)
gtest_discover_tests(bar_describe_tests)

add_executable(continuous_tool_tests continuous_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/ContinuousTool.cpp ${CMAKE_SOURCE_DIR}/src/app/ContinuousDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(continuous_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(continuous_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::Test GTest::gtest)
gtest_discover_tests(continuous_tool_tests)

add_executable(shell_tool_tests shell_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/ShellTool.cpp ${CMAKE_SOURCE_DIR}/src/app/ShellDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/ViewIcons.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp)
target_include_directories(shell_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(shell_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(shell_tool_tests)

add_executable(transform_tool_tests transform_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/TransformTool.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp
  ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/NodeCalculatorDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(transform_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(transform_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::Test GTest::gtest)
gtest_discover_tests(transform_tool_tests)

add_executable(import_tool_tests import_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/ImportTool.cpp ${CMAKE_SOURCE_DIR}/src/app/ImportDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(import_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_compile_definitions(import_tool_tests PRIVATE CC_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(import_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::Svg Qt6::Test GTest::gtest)
gtest_discover_tests(import_tool_tests)

add_executable(generate_tool_tests generate_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/GenerateTool.cpp ${CMAKE_SOURCE_DIR}/src/app/GenerateDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/ReferenceTool.cpp ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(generate_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(generate_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::Test GTest::gtest)
gtest_discover_tests(generate_tool_tests)

add_executable(support_dialogs_tests support_dialogs_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportTool.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(support_dialogs_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(support_dialogs_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(support_dialogs_tests)

add_executable(ligature_tool_tests ligature_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/LigatureTool.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(ligature_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(ligature_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(ligature_tool_tests)

add_executable(diaphragm_tool_tests diaphragm_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/DiaphragmTool.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(diaphragm_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(diaphragm_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(diaphragm_tool_tests)

add_executable(nodal_load_tool_tests nodal_load_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/NodalLoadTool.cpp ${CMAKE_SOURCE_DIR}/src/app/NodalLoadDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/DiaphragmTool.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(nodal_load_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(nodal_load_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(nodal_load_tool_tests)

add_executable(bar_ends_tool_tests bar_ends_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/BarEndsTool.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(bar_ends_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(bar_ends_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(bar_ends_tool_tests)

add_executable(buckling_tool_tests buckling_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/BucklingTool.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(buckling_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(buckling_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(buckling_tool_tests)

add_executable(deflection_tool_tests deflection_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/DeflectionTool.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(deflection_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(deflection_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(deflection_tool_tests)

add_executable(shell_properties_tool_tests shell_properties_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/ShellPropertiesTool.cpp ${CMAKE_SOURCE_DIR}/src/app/ShellPropertiesDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/ShellBandsDialog.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportDialogs.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/RangedEdit.cpp ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(shell_properties_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(shell_properties_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(shell_properties_tool_tests)

add_executable(loadcase_dialog_tests loadcase_dialog_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/LoadCaseDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(loadcase_dialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(loadcase_dialog_tests PRIVATE cc_core Qt6::Widgets Qt6::Svg GTest::gtest)
gtest_discover_tests(loadcase_dialog_tests)

add_executable(bar_load_tool_tests bar_load_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/BarLoadTool.cpp ${CMAKE_SOURCE_DIR}/src/app/BarLoadDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/NodalLoadTool.cpp ${CMAKE_SOURCE_DIR}/src/app/NodalLoadDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/DiaphragmTool.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(bar_load_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(bar_load_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(bar_load_tool_tests)

add_executable(shell_load_tool_tests shell_load_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/ShellLoadTool.cpp ${CMAKE_SOURCE_DIR}/src/app/ShellLoadDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/NodalLoadTool.cpp ${CMAKE_SOURCE_DIR}/src/app/NodalLoadDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/DiaphragmTool.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(shell_load_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(shell_load_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(shell_load_tool_tests)

add_executable(panel_tool_tests panel_tool_tests.cpp ${CMAKE_SOURCE_DIR}/src/app/PanelTool.cpp ${CMAKE_SOURCE_DIR}/src/app/PanelDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/NodalLoadTool.cpp ${CMAKE_SOURCE_DIR}/src/app/NodalLoadDialogs.cpp
  ${CMAKE_SOURCE_DIR}/src/app/DiaphragmTool.cpp ${CMAKE_SOURCE_DIR}/src/app/CypeDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SupportPictograms.cpp ${CMAKE_SOURCE_DIR}/src/app/Viewport3D.cpp ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_include_directories(panel_tool_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(panel_tool_tests PRIVATE cc_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg Qt6::PrintSupport Qt6::Test GTest::gtest)
gtest_discover_tests(panel_tool_tests)
