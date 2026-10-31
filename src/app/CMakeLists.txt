add_executable(cypeclwon WIN32 main.cpp MainWindow.cpp MainWindow.h SessionBar.cpp SessionBar.h
  WorkArea.cpp WorkArea.h
  StartPage.cpp StartPage.h
  CypeDialog.cpp CypeDialog.h NewProjectDialog.cpp NewProjectDialog.h
  ProjectTypeDialog.cpp ProjectTypeDialog.h
  StandardsDialog.cpp StandardsDialog.h
  FileManagerDialog.cpp FileManagerDialog.h LimitStatesDialog.cpp LimitStatesDialog.h WizardSubDialogs.cpp WizardSubDialogs.h OptionsDialogs.cpp OptionsDialogs.h EmptyProjectWizard.cpp EmptyProjectWizard.h
  ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_link_libraries(cypeclwon PRIVATE cc_core Qt6::Widgets Qt6::Svg SARibbonBar::SARibbonBar)


# i18n: source strings are Spanish (es-ES, MVP language); other languages are translated from them.
# Regenerate .ts files with: cmake --build <dir> --target update_translations
set(CC_TS_FILES ${CMAKE_SOURCE_DIR}/translations/cypeclwon_en_GB.ts)
set_source_files_properties(${CC_TS_FILES} PROPERTIES OUTPUT_LOCATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations)
qt_add_translations(cypeclwon TS_FILES ${CC_TS_FILES} SOURCE_TARGETS cypeclwon LRELEASE_TARGET release_translations)

