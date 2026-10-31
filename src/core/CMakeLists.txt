add_library(cc_core STATIC version.cpp standards.cpp standards.h)
target_include_directories(cc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(cc_core PUBLIC cxx_std_20)
target_compile_definitions(cc_core PUBLIC CC_VERSION_STRING="${PROJECT_VERSION}")

