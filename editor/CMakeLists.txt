cmake_minimum_required(VERSION 3.0)

project(nbsm_editor C)

add_compile_options(-Wall -Wextra -Wno-unknown-pragmas)

add_executable(nbsm_editor main.c editor.c save_load.c)

target_link_libraries(nbsm_editor ${RAYLIB_LIBRARY_PATH})
target_link_libraries(nbsm_editor ${JSONC_LIBRARY_PATH})

if (APPLE)
  target_link_libraries(nbsm_editor "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo")
endif (APPLE)

if (WIN32)
  target_link_libraries(nbsm_editor opengl32 gdi32 winmm)
endif (WIN32)

target_include_directories(nbsm_editor PUBLIC "${RAYLIB_INCLUDE_PATH}")
target_include_directories(nbsm_editor PUBLIC "${JSONC_INCLUDE_PATH}")

if (EMSCRIPTEN)
  set(ASYNCIFY_IMPORTS "[\"js_open_file_dialog\"]")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_GLFW=3 --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --js-library ${CMAKE_CURRENT_SOURCE_DIR}/js/api.js")
  set_target_properties(nbsm_editor PROPERTIES SUFFIX ".html")
endif(EMSCRIPTEN)