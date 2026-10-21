cmake_minimum_required(VERSION 3.16)
project(HimalayanWilds C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(third_party/glfw)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(HimalayanWilds ${GAME_SOURCES} third_party/glad/src/gl.c)
target_include_directories(HimalayanWilds PRIVATE
  src
  third_party/glad/include
  third_party/stb
  third_party/cgltf
  third_party/miniaudio)
target_link_libraries(HimalayanWilds PRIVATE glfw)

if(WIN32)
  if(MSVC)
    target_compile_options(HimalayanWilds PRIVATE /W3 /O2 /fp:fast /MP)
    target_compile_definitions(HimalayanWilds PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    set_target_properties(HimalayanWilds PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    target_link_options(HimalayanWilds PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
  else()
    target_compile_options(HimalayanWilds PRIVATE -O2 -ffast-math)
    target_link_options(HimalayanWilds PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
    target_compile_definitions(HimalayanWilds PRIVATE NOMINMAX)
  endif()
  target_link_libraries(HimalayanWilds PRIVATE winmm psapi)
else()
  target_compile_options(HimalayanWilds PRIVATE -O2 -ffast-math -Wall -Wno-unused-function -Wno-missing-field-initializers)
  find_package(Threads REQUIRED)
  target_link_libraries(HimalayanWilds PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

