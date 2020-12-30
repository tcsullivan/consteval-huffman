cmake_minimum_required(VERSION 3.14)

project(consteval_huffman
        VERSION 0.0.1
        DESCRIPTION "Compile-time Huffman coding compression using C++20"
        HOMEPAGE_URL "https://github.com/tcsullivan/consteval-huffman"
        LANGUAGES CXX)

# ---- Warning guard ----

# Protect dependents from this project's warnings if the guard isn't disabled
set(consteval_huffman_warning_guard SYSTEM)
if(consteval_huffman_INCLUDE_WITHOUT_SYSTEM)
  set(consteval_huffman_warning_guard "")
endif()

# ---- Declare library ----

add_library(consteval_huffman INTERFACE)
add_library(tcsullivan::consteval_huffman ALIAS consteval_huffman)

target_include_directories(consteval_huffman
        ${consteval_huffman_warning_guard}
        INTERFACE
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

target_compile_features(consteval_huffman INTERFACE cxx_std_20)

# ---- Install ----

include(CPack)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(consteval_huffman_directory "consteval_huffman-${PROJECT_VERSION}")
set(consteval_huffman_include_directory
        "${CMAKE_INSTALL_INCLUDEDIR}/${consteval_huffman_directory}")

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
        DESTINATION "${consteval_huffman_include_directory}"
        COMPONENT consteval_huffman_Development)

install(TARGETS consteval_huffman
        EXPORT consteval_huffmanTargets
        INCLUDES DESTINATION "${consteval_huffman_include_directory}")

write_basic_package_version_file(
        consteval_huffman-config-version.cmake
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)

set(consteval_huffman_install_cmakedir
        "${CMAKE_INSTALL_LIBDIR}/cmake/${consteval_huffman_directory}")

install(FILES "${PROJECT_BINARY_DIR}/consteval_huffman-config-version.cmake"
        DESTINATION "${consteval_huffman_install_cmakedir}"
        COMPONENT consteval_huffman_Package)

install(EXPORT consteval_huffmanTargets
        FILE consteval_huffman-config.cmake
        NAMESPACE tcsullivan::
        DESTINATION "${consteval_huffman_install_cmakedir}"
        COMPONENT consteval_huffman_Package)
