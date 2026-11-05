add_library(cc_core STATIC version.cpp standards.cpp standards.h project_settings.cpp project_settings.h
  limit_states.cpp limit_states.h recent_files.cpp recent_files.h undo_stack.cpp undo_stack.h units.cpp units.h
  options_settings.cpp options_settings.h
  combination_report.cpp combination_report.h combination_report_data.cpp combination_report_data.h
  combination_export.cpp combination_export.h combination_docx.cpp
  project_archive.cpp project_archive.h project.h project_json.cpp project_json.h project_file.cpp project_file.h
  structure_model.cpp structure_model.h general_data.cpp general_data.h camera.cpp camera.h selection.cpp selection.h picking.cpp picking.h snap.cpp snap.h display_options.cpp display_options.h categories.cpp categories.h clipping.cpp clipping.h work_planes.cpp work_planes.h node_edit.cpp node_edit.h bar_edit.cpp bar_edit.h piece_edit.cpp piece_edit.h structure_snapshot.cpp structure_snapshot.h geom3.h shell_edit.cpp shell_edit.h tools_edit.cpp tools_edit.h import_geometry.cpp import_geometry.h import_text.cpp import_text.h import_dxf.cpp import_dxf.h continuous_model.cpp continuous_model.h continuous_edit.cpp continuous_edit.h
  section_properties.cpp section_properties.h profile_series.cpp profile_series.h profile_library.cpp profile_library.h
  bar_description.cpp bar_description.h bar_element_data.cpp bar_element_data.h tie_document.cpp tie_document.h bar_description_edit.cpp bar_description_edit.h
  predefined_profiles.cpp predefined_profiles.h profile_catalogue_data.inc
  describir_catalogue.cpp describir_catalogue.h describir_catalogue_data.inc describir_tie_data.inc describir_concrete_data.inc describir_timber_data.inc tie_document_data.inc)
target_link_libraries(cc_core PRIVATE miniz::miniz nlohmann_json::nlohmann_json)
target_include_directories(cc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(cc_core PUBLIC cxx_std_20)
target_compile_definitions(cc_core PUBLIC CC_VERSION_STRING="${PROJECT_VERSION}")

