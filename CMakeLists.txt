cmake_minimum_required(VERSION 3.25)
project(CypeClwon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets PrintSupport OpenGLWidgets Svg Test LinguistTools)
find_package(GTest CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(miniz CONFIG REQUIRED)

set(SARIBBON_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(SARIBBON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SARibbon EXCLUDE_FROM_ALL)

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)



