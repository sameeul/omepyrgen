cmake_minimum_required(VERSION 3.20)
project(TestTS)
#==== Compiler Options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
  endif()
  
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(SOURCE 
           src/ts_driver/tiled_tiff/omexml.cc
           src/ts_driver/tiled_tiff/tiled_tiff_key_value_store.cc
           src/ts_driver/ometiff/metadata.cc
           src/ts_driver/ometiff/driver.cc
           src/zarr_pyramid_writer.cpp
)
add_executable(test_pyrgen ${SOURCE} tests/main.cpp)

include(FetchContent)

FetchContent_Declare(
  tensorstore
  URL "https://github.com/google/tensorstore/archive/refs/tags/v0.1.31.tar.gz"
  URL_HASH SHA256=b96ebf7151331eb273d08b1d9b82d1c7fc488a7754e690ba8fd14f6bda240f8f
)

# Additional FetchContent_Declare calls as needed...

FetchContent_MakeAvailable(tensorstore)

# Define a target that depends on TensorStore...


#include_directories("/mnt/hdd8/axle/dev/ts_cmake/test_proj/build/_deps")
include_directories(lib/pugixml)
include_directories(lib/bs_threadpool)
target_link_libraries(
  test_pyrgen
  PRIVATE
    tensorstore::tensorstore tensorstore::all_drivers
)