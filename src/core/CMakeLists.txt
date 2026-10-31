add_library(cc_core STATIC version.cpp standards.cpp standards.h project_settings.cpp project_settings.h
  limit_states.cpp limit_states.h recent_files.cpp recent_files.h undo_stack.cpp undo_stack.h
  options_settings.cpp options_settings.h
  combination_report.cpp combination_report.h combination_report_data.cpp combination_report_data.h
  combination_export.cpp combination_export.h combination_docx.cpp
  project_archive.cpp project_archive.h)
target_link_libraries(cc_core PRIVATE miniz::miniz)
target_include_directories(cc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(cc_core PUBLIC cxx_std_20)
target_compile_definitions(cc_core PUBLIC CC_VERSION_STRING="${PROJECT_VERSION}")

