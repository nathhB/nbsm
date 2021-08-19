cmake_minimum_required(VERSION 3.0)

project(nbsm_editor C)

add_compile_options(-Wall -Wextra -Wno-unknown-pragmas)

add_executable(nbsm_editor editor.c)

find_library(RAYLIB_LIBRARY raylib HINTS ${RAYLIB_LIBRARY_PATH} REQUIRED)

message("Found raylib library: ${RAYLIB_LIBRARY}")

target_link_libraries(nbsm_editor ${RAYLIB_LIBRARY})

if (APPLE)
  target_link_libraries(nbsm_editor "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo")
endif (APPLE)

if (WIN32)
  target_link_libraries(nbsm_editor opengl32 gdi32 winmm)
endif (WIN32)

target_include_directories(nbsm_editor PUBLIC "${RAYLIB_INCLUDE_PATH}")
