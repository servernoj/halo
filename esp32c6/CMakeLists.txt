cmake_minimum_required(VERSION 3.22)

# Get git commit hash for version identification
execute_process(
  COMMAND git rev-parse --short=7 HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

# Set project version to git hash (or "unknown" if not in git repo)
if(GIT_HASH)
  set(PROJECT_VER "${GIT_HASH}")
else()
  set(PROJECT_VER "unknown")
endif()

set(EXTRA_COMPONENT_DIRS components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options(-Wno-missing-field-initializers)
project(halo)
