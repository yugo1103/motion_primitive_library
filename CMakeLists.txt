cmake_minimum_required(VERSION 2.8)
project(motion_primitive_library)

# set default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wno-deprecated-declarations")

set(BUILD_SHARED_LIBS ON)

find_package(Eigen3 REQUIRED)
find_package(PkgConfig REQUIRED)
PKG_CHECK_MODULES(YAMLCPP REQUIRED yaml-cpp)
include_directories(${EIGEN3_INCLUDE_DIR} include)

if(YAMLCPP_FOUND)
  message(STATUS "Found yaml-cpp.")
else()
  message(FATAL_ERROR "Could not find yaml-cpp.")
endif()

add_library(poly_solver src/mpl_traj_solver/poly_solver.cpp
  src/mpl_traj_solver/poly_traj.cpp)

add_library(map_planner src/mpl_planner/map_planner.cpp)

include(CTest)

install(FILES "${PROJECT_NAME}Config.cmake" "${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION "share/${PROJECT_NAME}/cmake")

install(TARGETS poly_solver map_planner
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

install(DIRECTORY include/mpl_basis include/mpl_traj_solver include/mpl_planner include/mpl_collision
  DESTINATION include
  )
