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
           src/ome_tiff_to_zarr_converter.cpp
           src/zarr_pyramid_assembler.cpp
           src/zarr_base_to_pyr_gen.cpp
           src/ome_tiff_to_zarr_pyramid.cpp
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

include_directories(lib/pugixml)
include_directories(lib/bs_threadpool)
target_link_libraries(
  test_pyrgen
  PRIVATE
    tensorstore::tensorstore tensorstore::all_drivers
)

find_package(Threads QUIET)
if (Threads_FOUND)
    if (CMAKE_USE_PTHREADS_INIT)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    endif (CMAKE_USE_PTHREADS_INIT)
    list(APPEND Build_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
else ()
	message(STATUS "Unable to find threads. Nyxus must have a threading library i.e. pthreads.")
endif ()

# Python bindings.
# PyBind
add_subdirectory(lib/pybind11)

pybind11_add_module(libomepyrgen 
  ${SOURCE}
  src/python/pyramid_python_interface.cpp
)

target_compile_definitions(libomepyrgen PRIVATE WITH_PYTHON_H)
target_link_libraries(libomepyrgen PRIVATE stdc++fs)
target_link_libraries(libomepyrgen PRIVATE tensorstore::tensorstore tensorstore::all_drivers)
target_link_libraries(libomepyrgen PRIVATE ${Build_LIBRARIES})  

target_link_libraries(
  test_pyrgen
  PRIVATE
    ${Build_LIBRARIES}
)