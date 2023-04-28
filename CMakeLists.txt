cmake_minimum_required(VERSION 3.20)

include(cmake/prelude.cmake)

project(
    splittable
    VERSION 0.1.0
    DESCRIPTION "Short description"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Import dependencies ----

include(FetchContent)

FetchContent_Declare(
  wyatt-stm
  GIT_REPOSITORY https://github.com/bretthall/Wyatt-STM
  GIT_TAG        8c53c063c30a1b0284ce48a96431580028c72d39
)

FetchContent_MakeAvailable(wyatt-stm)

# ---- Declare library ----

add_library(
    splittable_splittable
    source/splittable.cpp
)
add_library(splittable::splittable ALIAS splittable_splittable)

include(GenerateExportHeader)
generate_export_header(
    splittable_splittable
    BASE_NAME splittable
    EXPORT_FILE_NAME export/splittable/splittable_export.hpp
    CUSTOM_CONTENT_FROM_VARIABLE pragma_suppress_c4251
)

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(splittable_splittable PUBLIC SPLITTABLE_STATIC_DEFINE)
endif()

set_target_properties(
    splittable_splittable PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    EXPORT_NAME splittable
    OUTPUT_NAME splittable
)

target_include_directories(
    splittable_splittable ${warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_include_directories(
    splittable_splittable SYSTEM
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/export>"
)

target_compile_features(splittable_splittable PUBLIC cxx_std_23)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT splittable_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of splittable"
  )
endif()

include(cmake/dev-mode.cmake)
