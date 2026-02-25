cmake_minimum_required(VERSION 3.14)
project(ftr C CXX)

# Library
add_library(ftr SHARED src/ftr.c)
target_include_directories(ftr PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)
set_target_properties(ftr PROPERTIES C_STANDARD 11)

# Examples — build every .cpp in examples/
option(FTR_BUILD_EXAMPLES "Build example programs" ON)
if(FTR_BUILD_EXAMPLES)
  file(GLOB EXAMPLE_SRCS examples/*.cpp)
  foreach(src ${EXAMPLE_SRCS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE ftr)
    set_target_properties(${name} PROPERTIES CXX_STANDARD 17)
  endforeach()
endif()

# Install
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ftr EXPORT ftrTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES src/ftr.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ftrTargets
  FILE ftrTargets.cmake
  NAMESPACE ftr::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ftr)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ftrConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ftrConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ftr)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ftrConfig.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ftr)
