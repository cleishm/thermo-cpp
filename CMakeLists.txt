cmake_minimum_required(VERSION 3.14)

project(thermo
    VERSION 1.0.0
    DESCRIPTION "Type-safe temperature handling library modeled after std::chrono"
    HOMEPAGE_URL "https://github.com/cleishm/thermo-cpp"
    LANGUAGES CXX
)

option(THERMO_BUILD_TESTS "Build tests" ${thermo_IS_TOP_LEVEL})

# Main library target (header-only)
add_library(thermo INTERFACE)
add_library(thermo::thermo ALIAS thermo)

target_include_directories(thermo INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(thermo INTERFACE cxx_std_23)

# Tests
if(THERMO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS thermo
    EXPORT thermoTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT thermoTargets
    FILE thermoTargets.cmake
    NAMESPACE thermo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thermo
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/thermoConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/thermoConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thermo
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/thermoConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/thermoConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/thermoConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thermo
)
