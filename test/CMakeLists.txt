cmake_minimum_required(VERSION 3.12)

project(string_view_tests LANGUAGES CXX)

# ---- Add root project ----

# Enable warnings from includes
set(string_view_INCLUDE_WITHOUT_SYSTEM ON CACHE INTERNAL "")
add_subdirectory("${PROJECT_SOURCE_DIR}/.." "${PROJECT_BINARY_DIR}/root_project")

# ---- Test ----

enable_testing()

add_executable(sv_test tests.cpp)

target_link_libraries(sv_test PRIVATE bev::string_view)
target_compile_features(sv_test PRIVATE cxx_std_17)

add_test(NAME string_view.main COMMAND sv_test)
# At the moment the test consists of returning a comparison result, which is
# expected to be true, i.e. a non-zero result
set_property(TEST string_view.main PROPERTY WILL_FAIL YES)
