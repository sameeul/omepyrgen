#include "zarr_pyramid_writer.h"
#include "downsample.h"

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

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <filesystem>
namespace fs = std::filesystem;

using ::tensorstore::Context;
using ::tensorstore::internal_zarr::ChooseBaseDType;
using namespace std::chrono_literals;


void ZarrPyramidWriter::CreateBaseZarrImage(){
  
  
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
                                          {"shape", {1, _base_length, _base_width}},
                                          {"chunks", {1, chunk_size, chunk_size}},
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
      _th_pool.push_task([&store1, &store2, x_start, x_end, y_start, y_end](){  
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
                                  tensorstore::Dims(0).ClosedInterval(0,0) |
                                  tensorstore::Dims(1).ClosedInterval(x_start,x_end-1) |
                                  tensorstore::Dims(2).ClosedInterval(y_start,y_end-1)).value();     

      });       

     }
  }
  _th_pool.wait_for_tasks();
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
                                  tensorstore::Dims(0).ClosedInterval(0,0) |
                                  tensorstore::Dims(1).ClosedInterval(x_start,x_end-1) |
                                  tensorstore::Dims(2).ClosedInterval(y_start,y_end-1)).value();    

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

void ZarrPyramidWriter::CreatePyramidImages()
{
    for (int i=_max_level; i>_min_level; --i){
      std::string input_path = _output_file + "/" + std::to_string(i) + "/";
      std::string output_path = _output_file + "/" + std::to_string(i-1) + "/";
      WriteDownsampledImage<uint16_t>(input_path, output_path);
  }
}
void ZarrPyramidWriter::WriteMultiscaleMetadata()
{
  json zarr_multiscale_axes;
  zarr_multiscale_axes = json::parse(R"([
                {"name": "z", "type": "space", "unit": "micrometer"},
                {"name": "y", "type": "space", "unit": "micrometer"},
                {"name": "x", "type": "space", "unit": "micrometer"}
            ])");
  
 
  float level = 1.0;
  json scale_metadata_list = json::array();
  for(int i=_max_level; i>=_min_level; i--){
    json scale_metadata;
    scale_metadata["path"] = std::to_string(i);
    scale_metadata["coordinateTransformations"] = {{{"type", "scale"}, {"scale", {1.0, level, level}}}};
    scale_metadata_list.push_back(scale_metadata);
    level = level*2;
  }

  json combined_metadata;
  combined_metadata["datasets"] = scale_metadata_list;
  combined_metadata["version"] = "0.4";
  combined_metadata["axes"] = zarr_multiscale_axes;
  combined_metadata["name"] = fs::path(_input_file).filename();
  combined_metadata["metadata"] = {{"method", "mean"}};
  json final_formated_metadata;
  final_formated_metadata["multiscales"] = {combined_metadata};
  std::ofstream f(_output_file + "/.zattrs",std::ios_base::trunc |std::ios_base::out);
  f << final_formated_metadata;

}
template <typename T>
void ZarrPyramidWriter::WriteDownsampledImage(std::string &base_file, std::string &downsampled_file)
{
  tensorstore::Context context = Context::Default();
  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store1, tensorstore::Open({{"driver", "zarr"},
                          {"kvstore", {{"driver", "file"},
                                        {"path", base_file}}
                          },
                          {"context", {
                            {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                            {"data_copy_concurrency", {{"limit", 4}}},
                            {"file_io_concurrency", {{"limit", 4}}},
                          }}},
                          //context,
                          tensorstore::OpenMode::open,
                          tensorstore::ReadWriteMode::read).result());
  auto shape = store1.domain().shape();

  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                     ChooseBaseDType(store1.dtype()));
  auto prev_x_max = static_cast<std::int64_t>(shape[1]);
  auto prev_y_max = static_cast<std::int64_t>(shape[2]);

  auto cur_x_max = static_cast<std::int64_t>(ceil(prev_x_max/2.0));
  auto cur_y_max = static_cast<std::int64_t>(ceil(prev_y_max/2.0));

  auto num_rows = static_cast<std::int64_t>(ceil(1.0*cur_x_max/chunk_size));
  auto num_cols = static_cast<std::int64_t>(ceil(1.0*cur_y_max/chunk_size));

  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store2, tensorstore::Open({{"driver", "zarr"},
                          {"kvstore", {{"driver", "file"},
                                        {"path", downsampled_file}}
                          },
                          {"context", {
                            {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                            {"data_copy_concurrency", {{"limit", 4}}},
                            {"file_io_concurrency", {{"limit", 4}}},
                          }},
                          {"metadata", {
                                        {"zarr_format", 2},
                                        {"shape", {1, cur_x_max, cur_y_max}},
                                        {"chunks", {1, chunk_size, chunk_size}},
                                        {"dtype", base_zarr_dtype.encoded_dtype},
                                        },
                          }},
                          tensorstore::OpenMode::create |
                          tensorstore::OpenMode::delete_existing,
                          tensorstore::ReadWriteMode::write).result());

  for(std::int64_t i=0; i<num_rows; ++i){
    auto x_start = i*chunk_size;
    auto x_end = std::min({(i+1)*chunk_size, cur_x_max});

    auto prev_x_start = 2*x_start;
    auto prev_x_end = std::min({2*x_end, prev_x_max});
    for(std::int64_t j=0; j<num_cols; ++j){
      auto y_start = j*chunk_size;
      auto y_end = std::min({(j+1)*chunk_size, cur_y_max});
      auto prev_y_start = 2*y_start;
      auto prev_y_end = std::min({2*y_end, prev_y_max});

      _th_pool.push_task([store1, store2, x_start, x_end, y_start, y_end, prev_x_start, prev_x_end, prev_y_start, prev_y_end](){  
                    std::vector<T> read_buffer((prev_x_end-prev_x_start)*(prev_y_end-prev_y_start));
                    auto array = tensorstore::Array(read_buffer.data(), {prev_x_end-prev_x_start, prev_y_end-prev_y_start}, tensorstore::c_order);

                    tensorstore::Read(store1 |
                            tensorstore::Dims(0).ClosedInterval(0,0) | 
                            tensorstore::Dims(1).ClosedInterval(prev_x_start,prev_x_end-1) |
                            tensorstore::Dims(2).ClosedInterval(prev_y_start,prev_y_end-1) ,
                            tensorstore::UnownedToShared(array)).value();

                    auto result = DownsampleAverage(read_buffer, (prev_x_end-prev_x_start), (prev_y_end-prev_y_start));
                    auto result_array = tensorstore::Array(result->data(), {x_end-x_start, y_end-y_start}, tensorstore::c_order);
                    tensorstore::Write(tensorstore::UnownedToShared(result_array), store2 |
                        tensorstore::Dims(0).ClosedInterval(0,0) | 
                        tensorstore::Dims(1).ClosedInterval(x_start,x_end-1) |
                        tensorstore::Dims(2).ClosedInterval(y_start,y_end-1)).value();     

      }); 
    }
  }
  _th_pool.wait_for_tasks();
}
