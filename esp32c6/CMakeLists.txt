cmake_minimum_required(VERSION 3.22)

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE GIT_RESULT
)
if(GIT_RESULT EQUAL 0 AND GIT_COMMIT_HASH)
  set(PROJECT_VER ${GIT_COMMIT_HASH})
  message(STATUS "Git commit hash: ${GIT_COMMIT_HASH}")
else()
  # Fallback to GitHub Actions environment variables
  if(DEFINED ENV{GITHUB_SHA})
    string(SUBSTRING $ENV{GITHUB_SHA} 0 7 GITHUB_SHA_SHORT)
    set(PROJECT_VER ${GITHUB_SHA_SHORT})
    message(STATUS "Using GitHub SHA: ${GITHUB_SHA_SHORT}")
  else()
    set(PROJECT_VER "unknown")
    message(WARNING "Failed to get git commit hash (result: ${GIT_RESULT}) and no GITHUB_SHA available, using 'unknown'")
  endif()
endif()

set(EXTRA_COMPONENT_DIRS components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options(-Wno-missing-field-initializers)
project(halo)
