#include "zarr_pyramid_assembler.h"
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

#include <unordered_set>
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
#include <thread>


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


void ZarrPyramidAssembler::CreateBaseZarrImage(BS::thread_pool& th_pool)
{
  std::regex match_str(
      "file:\\s(\\S+);[\\s|\\S]+position:\\s\\((\\d+),\\s(\\d+)\\);[\\s|\\S]+grid:\\s\\((\\d+),\\s(\\d+)\\);");
  
  std::fstream stitch_vec;
  stitch_vec.open(_stitching_file, std::ios::in);
  std::string line;
  std::smatch pieces_match;
  std::vector<ImageSegment> image_vec;
  // find final image size
  std::int64_t image_x_max=0, image_y_max=0, grid_x_max=0, grid_y_max=0; 
  ImageSegment img_00, img_01, img_10;

  while (getline(stitch_vec, line)) {
    if (std::regex_match(line, pieces_match, match_str)) {
      if(pieces_match.size() == 6){
        auto fname = pieces_match[1].str();
        auto x = static_cast<std::int64_t>(std::stoi(pieces_match[2].str()));
        auto y = static_cast<std::int64_t>(std::stoi(pieces_match[3].str()));
        auto gx = std::stoi(pieces_match[4].str());
        auto gy = std::stoi(pieces_match[5].str());

        (image_x_max < x) ? image_x_max = x: image_x_max = image_x_max;  
        (image_y_max < y) ? image_y_max = y: image_y_max = image_y_max;
        image_vec.emplace_back(fname, x, y);   

        if (gx==0 && gy ==0){
            img_00.file_name = fname;
            img_00.x_pos = x;
            img_00.y_pos = y;
        }
        if (gx==0 && gy ==1){
            img_01.file_name = fname;
            img_01.x_pos = x;
            img_01.y_pos = y;
        }
        if (gx==1 && gy ==0){
            img_10.file_name = fname;
            img_10.x_pos = x;
            img_10.y_pos = y;
        }
      }
    }

  }



    std::cout<<image_vec.size()<<std::endl;
    std::cout<<"x_max = "<<image_x_max<<std::endl;
    std::cout<<"y_max = "<<image_y_max<<std::endl;
    std::cout<<"image width = "<<img_01.y_pos - img_00.y_pos << std::endl;
    std::cout<<"image height = "<<img_10.x_pos - img_00.x_pos << std::endl;

    std::int64_t full_image_width = image_x_max + img_10.x_pos - img_00.x_pos;
    std::int64_t full_image_height = image_y_max + img_01.y_pos - img_00.y_pos;

    std::cout<<"full image width = "<< full_image_width<<std::endl;
    std::cout<<"full image height = "<< full_image_height<<std::endl;

    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto test_source, tensorstore::Open({{"driver", "ometiff"},

                                {"kvstore", {{"driver", "tiled_tiff"},
                                            {"path", _input_dir + "/" + image_vec[0].file_name}}
                                },
                                // {"context", {
                                // {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                                // {"data_copy_concurrency", {{"limit", 8}}},
                                // {"file_io_concurrency", {{"limit", 8}}},
                                // }},
                                },
                                //context,
                                tensorstore::OpenMode::open,
                                tensorstore::ReadWriteMode::read).result());

    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                        ChooseBaseDType(test_source.dtype()));

    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto dest, tensorstore::Open({{"driver", "zarr"},
                                {"kvstore", {{"driver", "file"},
                                            {"path", _output_file}}
                                },
                                {"context", {
                                {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                                {"data_copy_concurrency", {{"limit", 8}}},
                                {"file_io_concurrency", {{"limit", 8}}},
                                }},
                                {"metadata", {
                                            {"zarr_format", 2},
                                            {"shape", {1, full_image_height, full_image_width}},
                                            {"chunks", {1, chunk_size, chunk_size}},
                                            {"dtype", base_zarr_dtype.encoded_dtype},
                                            {"compressor", {
                                              {"id","blosc"}, 
                                              {"cname", "zstd"}, 
                                              {"clevel", 3}, 
                                              {"shuffle", 2},
                                              }},
                                            },
                                }},
                                //context,
                                tensorstore::OpenMode::create |
                                tensorstore::OpenMode::delete_existing,
                                tensorstore::ReadWriteMode::write).result());

    // bind mem pool with thread pool

    auto mem_pool = std::unordered_map<std::thread::id, std::vector<uint16_t>>();
    for (int i; i< 4; i++){
        th_pool.push_task([&mem_pool](){
        mem_pool.emplace(std::this_thread::get_id(), std::vector<uint16_t>(1080*1080));
      });

    }

    th_pool.wait_for_tasks();

    for(const auto& i: image_vec){

        //std::string inp_path = _input_dir;
        th_pool.push_task([&dest, i, &mem_pool, this](){
        //std::cout<<"thread id "<<std::this_thread::get_id()<<std::endl;
        //thread_set.emplace(std::this_thread::get_id());  
           //std::cout<< inp_path<<std::endl;
            //std::cout<<"filename: "<< i.file_name << " x pos: " << i.x_pos << " y pos: "<< i.y_pos<<std::endl;
        TENSORSTORE_CHECK_OK_AND_ASSIGN(auto source, tensorstore::Open({{"driver", "ometiff"},

                                    {"kvstore", {{"driver", "tiled_tiff"},
                                                {"path", this->_input_dir + "/" + i.file_name}}
                                    }},
                                    //context,
                                    tensorstore::OpenMode::open,
                                    tensorstore::ReadWriteMode::read).result());
        
        auto image_shape = source.domain().shape();
        auto image_width = image_shape[4];
        auto image_height = image_shape[3];
        //auto buf = std::vector<uint16_t>(1090*1090);
        auto& buf = mem_pool[std::this_thread::get_id()];
        //std::cout << buf.size() << std::endl;
        auto array = tensorstore::Array(buf.data(), {image_height, image_width}, tensorstore::c_order);
        //auto array = tensorstore::AllocateArray({image_height, image_width},tensorstore::c_order,
        //                                                    tensorstore::value_init, source.dtype());
        // initiate a read
        tensorstore::Read(source | 
                tensorstore::Dims(0).ClosedInterval(0,0) |
                tensorstore::Dims(1).ClosedInterval(0,0) |
                tensorstore::Dims(2).ClosedInterval(0,0) |
                tensorstore::Dims(3).ClosedInterval(0, image_height-1) |
                tensorstore::Dims(4).ClosedInterval(0, image_width-1) ,
                tensorstore::UnownedToShared(array)).value();
        
        //std::cout << "we read " << i.file_name << std::endl;
        tensorstore::Write(tensorstore::UnownedToShared(array), dest |
            tensorstore::Dims(0).ClosedInterval(0,0) |
            tensorstore::Dims(1).ClosedInterval(i.y_pos,i.y_pos+image_height-1) |
            tensorstore::Dims(2).ClosedInterval(i.x_pos,i.x_pos+image_width-1)).value();  
        });
        
 

    }

    th_pool.wait_for_tasks();
//     auto total_writes {pending_writes.size()};
//   while(total_writes > 0){
//     for(auto &f: pending_writes){
//       if(f.valid()){
//         auto status = f.wait_for(10ms);
//         if (status == std::future_status::ready){
//           auto tmp = f.get();
//           --total_writes;
//         }
//       }
//     }
//   }
}