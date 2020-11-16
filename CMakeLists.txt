cmake_minimum_required(VERSION 3.12)

project(string_view
        VERSION 1.0.0
        DESCRIPTION "A string_view implementation that can remember if it was a c-string once"
        HOMEPAGE_URL "https://github.com/lava/string_view"
        LANGUAGES CXX)

# ---- Warning guard ----

# Protect dependents from this project's warnings if the guard isn't disabled
set(string_view_warning_guard "SYSTEM")
if(string_view_INCLUDE_WITHOUT_SYSTEM)
  set(string_view_warning_guard "")
endif()

# ---- Declare library ----

add_library(string_view INTERFACE)
add_library(bev::string_view ALIAS string_view)

target_include_directories(string_view
        ${string_view_warning_guard}
        INTERFACE
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

target_compile_features(string_view INTERFACE cxx_std_17)

# ---- Install ----

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(string_view_directory "string_view-${PROJECT_VERSION}")
set(string_view_include_directory
        "${CMAKE_INSTALL_INCLUDEDIR}/${string_view_directory}")

install(TARGETS string_view
        EXPORT string_viewTargets
        INCLUDES DESTINATION "${string_view_include_directory}")

set(string_view_install_cmakedir
        "${CMAKE_INSTALL_LIBDIR}/cmake/${string_view_directory}")

write_basic_package_version_file(
        string_viewConfigVersion.cmake
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)

install(EXPORT string_viewTargets
        FILE string_viewConfig.cmake
        NAMESPACE bev::
        DESTINATION "${string_view_install_cmakedir}")

install(FILES
        "${PROJECT_BINARY_DIR}/string_viewConfigVersion.cmake"
        DESTINATION "${string_view_install_cmakedir}")

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
        DESTINATION "${string_view_include_directory}")

