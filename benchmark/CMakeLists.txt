cmake_minimum_required(VERSION 2.8)

PROJECT(Benchmark)

find_package(VTK REQUIRED)

include_directories(../)

add_executable(benchmark benchmark.cxx ../Render.h ../Render.cxx)

if(VTK_LIBRARIES)
  target_link_libraries(benchmark ${VTK_LIBRARIES})
else()
  target_link_libraries(benchmark vtkHybrid vtkWidgets)
endif()
