cmake_minimum_required(VERSION 3.14)

project(splittableTests LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)
include(../cmake/windows-set-path.cmake)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
  find_package(splittable REQUIRED)
  enable_testing()
endif()

# ---- Tests ----

add_executable(splittable_test source/splittable_test.cpp)
target_link_libraries(splittable_test PRIVATE splittable::splittable)
target_compile_features(splittable_test PRIVATE cxx_std_20)

add_test(NAME splittable_test COMMAND splittable_test)
windows_set_path(splittable_test splittable::splittable)

# ---- End-of-file commands ----

add_folders(Test)
