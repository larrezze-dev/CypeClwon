add_executable(cypeclwon WIN32 main.cpp MainWindow.cpp MainWindow.h)
target_link_libraries(cypeclwon PRIVATE cc_core Qt6::Widgets SARibbonBar::SARibbonBar)

