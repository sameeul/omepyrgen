#include "zarr_pyramid_writer.h"

#include <string>
#include <unistd.h>
#include <stdint.h>
#include<typeinfo>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>
#include <chrono>
#include <future>
#include <cmath>

#include "tensorstore/tensorstore.h"
#include "tensorstore/context.h"
#include "tensorstore/array.h"
#include "tensorstore/driver/zarr/dtype.h"
#include "tensorstore/index_space/dim_expression.h"
#include "tensorstore/kvstore/kvstore.h"
#include "tensorstore/open.h"

#include "BS_thread_pool.hpp"

using ::tensorstore::Context;
using ::tensorstore::internal_zarr::ChooseBaseDType;
using namespace std::chrono_literals;


void ZarrPyramidWriter::CreateBaseZarrImage(){
  std::int64_t chunk_size = 1024;
  BS::thread_pool pool(4);
  tensorstore::Context context = Context::Default();
  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store1, tensorstore::Open({{"driver", "ometiff"},

                            {"kvstore", {{"driver", "tiled_tiff"},
                                         {"path", _input_file}}
                            },
                            {"context", {
                              {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                              {"data_copy_concurrency", {{"limit", 4}}},
                              {"file_io_concurrency", {{"limit", 4}}},
                            }},
                            },
                            //context,
                            tensorstore::OpenMode::open,
                            tensorstore::ReadWriteMode::read).result());

  auto shape = store1.domain().shape();
  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                     ChooseBaseDType(store1.dtype()));
  _base_length = shape[3];
  _base_width = shape[4];
  _max_level = static_cast<int>(ceil(log2(std::max({_base_length, _base_width}))));
  _min_level = static_cast<int>(ceil(log2(1024)));
  std::string base_zarr_file = _output_file+"/" + std::to_string(_max_level)+"/";
  auto num_rows = static_cast<std::int64_t>(ceil(1.0*_base_length/chunk_size));
  auto num_cols = static_cast<std::int64_t>(ceil(1.0*_base_width/chunk_size));
  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store2, tensorstore::Open({{"driver", "zarr"},
                            {"kvstore", {{"driver", "file"},
                                         {"path", base_zarr_file}}
                            },
                            {"context", {
                              {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                              {"data_copy_concurrency", {{"limit", 4}}},
                              {"file_io_concurrency", {{"limit", 4}}},
                            }},
                            {"metadata", {
                                          {"zarr_format", 2},
                                          {"shape", {_base_length, _base_width}},
                                          {"chunks", {chunk_size, chunk_size}},
                                          {"dtype", base_zarr_dtype.encoded_dtype},
                                          },
                            }},
                            //context,
                            tensorstore::OpenMode::create |
                            tensorstore::OpenMode::delete_existing,
                            tensorstore::ReadWriteMode::write).result());

  for(std::int64_t i=0; i<num_rows; ++i){
    std::int64_t x_start = i*chunk_size;
    std::int64_t x_end = std::min({(i+1)*chunk_size, _base_length});
    for(std::int64_t j=0; j<num_cols; ++j){
      std::int64_t y_start = j*chunk_size;
      std::int64_t y_end = std::min({(j+1)*chunk_size, _base_width});
      pool.push_task([&store1, &store2, x_start, x_end, y_start, y_end](){  
                              auto array = tensorstore::AllocateArray({x_end-x_start, y_end-y_start},tensorstore::c_order,
                                                            tensorstore::value_init, store1.dtype());
                              // initiate a read
                              tensorstore::Read(store1 | 
                                        tensorstore::Dims(0).ClosedInterval(0,0) |
                                        tensorstore::Dims(1).ClosedInterval(0,0) |
                                        tensorstore::Dims(2).ClosedInterval(0,0) |
                                        tensorstore::Dims(3).ClosedInterval(x_start,x_end-1) |
                                        tensorstore::Dims(4).ClosedInterval(y_start,y_end-1) ,
                                        array).value();
                                                
                              // initiate write
                              tensorstore::Write(array, store2 |
                                  tensorstore::Dims(0).ClosedInterval(x_start,x_end-1) |
                                  tensorstore::Dims(1).ClosedInterval(y_start,y_end-1)).value();     

      });       

     }
  }
  pool.wait_for_tasks();
}

void ZarrPyramidWriter::CreateBaseZarrImageV2(){
  std::int64_t chunk_size = 1024;
  tensorstore::Context context = Context::Default();
  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store1, tensorstore::Open({{"driver", "ometiff"},

                            {"kvstore", {{"driver", "tiled_tiff"},
                                         {"path", _input_file}}
                            }
                            },
                            //context,
                            tensorstore::OpenMode::open,
                            tensorstore::ReadWriteMode::read).result());

  auto shape = store1.domain().shape();
  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                     ChooseBaseDType(store1.dtype()));
  _base_length = shape[3];
  _base_width = shape[4];
  _max_level = static_cast<int>(ceil(log2(std::max({_base_length, _base_width}))));
  _min_level = static_cast<int>(ceil(log2(1024)));
  std::string base_zarr_file = _output_file+"/" + std::to_string(_max_level)+"/";
  auto num_rows = static_cast<std::int64_t>(ceil(1.0*_base_length/chunk_size));
  auto num_cols = static_cast<std::int64_t>(ceil(1.0*_base_width/chunk_size));
  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store2, tensorstore::Open({{"driver", "zarr"},
                            {"kvstore", {{"driver", "file"},
                                         {"path", base_zarr_file}}
                            },
                            {"context", {
                              {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                              {"data_copy_concurrency", {{"limit", 4}}},
                              {"file_io_concurrency", {{"limit", 4}}},
                            }},
                            {"metadata", {
                                          {"zarr_format", 2},
                                          {"shape", {_base_length, _base_width}},
                                          {"chunks", {chunk_size, chunk_size}},
                                          {"dtype", base_zarr_dtype.encoded_dtype},
                                          },
                            }},
                            //context,
                            tensorstore::OpenMode::create |
                            tensorstore::OpenMode::delete_existing,
                            tensorstore::ReadWriteMode::write).result());


  std::list<std::future<double>> pending_writes;

  for(std::int64_t i=0; i<num_rows; ++i){
    std::int64_t x_start = i*chunk_size;
    std::int64_t x_end = std::min({(i+1)*chunk_size, _base_length});
    for(std::int64_t j=0; j<num_cols; ++j){
      std::int64_t y_start = j*chunk_size;
      std::int64_t y_end = std::min({(j+1)*chunk_size, _base_width});      
      pending_writes.emplace_back(std::async([&store1, &store2, x_start, x_end, y_start, y_end](){  
                              auto array = tensorstore::AllocateArray({x_end-x_start, y_end-y_start},tensorstore::c_order,
                                                            tensorstore::value_init, store1.dtype());
                              // initiate a read
                              tensorstore::Read(store1 | 
                                        tensorstore::Dims(0).ClosedInterval(0,0) |
                                        tensorstore::Dims(1).ClosedInterval(0,0) |
                                        tensorstore::Dims(2).ClosedInterval(0,0) |
                                        tensorstore::Dims(3).ClosedInterval(x_start,x_end-1) |
                                        tensorstore::Dims(4).ClosedInterval(y_start,y_end-1) ,
                                        array).value();
                                                
                              // initiate write
                              tensorstore::Write(array, store2 |
                                  tensorstore::Dims(0).ClosedInterval(x_start,x_end-1) |
                                  tensorstore::Dims(1).ClosedInterval(y_start,y_end-1)).value();     

        return 0.0;
      })); 
    }
  }

  auto total_writes {pending_writes.size()};
  while(total_writes > 0){
    for(auto &f: pending_writes){
      if(f.valid()){
        auto status = f.wait_for(10ms);
        if (status == std::future_status::ready){
          auto tmp = f.get();
          --total_writes;
        }
      }
    }
  }
}