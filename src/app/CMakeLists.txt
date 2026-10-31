add_executable(cypeclwon WIN32 main.cpp MainWindow.cpp MainWindow.h SessionBar.cpp SessionBar.h
  ${CMAKE_SOURCE_DIR}/resources/app.qrc)
target_link_libraries(cypeclwon PRIVATE cc_core Qt6::Widgets Qt6::Svg SARibbonBar::SARibbonBar)

