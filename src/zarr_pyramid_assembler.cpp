#include "zarr_pyramid_assembler.h"
#include "pugixml.hpp"
#include "tiffio.h"
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>
#include <cstring>
#include <unordered_set>
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <typeinfo>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>
#include <chrono>
#include <future>
#include <cmath>
#include <thread>
#include <bits/stdc++.h>

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


OmeTiffCollToZarr::OmeTiffCollToZarr(const std::string& input_dir,
                      const std::string& stitching_file):
        _input_dir(input_dir),
        _stitching_file(stitching_file){
  std::regex match_str(
      "file:\\s(\\S+);[\\s|\\S]+position:\\s\\((\\d+),\\s(\\d+)\\);[\\s|\\S]+grid:\\s\\((\\d+),\\s(\\d+)\\);");
  

  std::string line;
  std::smatch pieces_match;

  // find final image size
  std::int64_t image_x_max=0, image_y_max=0, grid_x_max=0, grid_y_max=0; 
  ImageSegment img_00, img_01, img_10;

  std::fstream stitch_vec;
  stitch_vec.open(_stitching_file, std::ios::in);
  if(!stitch_vec.is_open()){
    std::cout<<"Stitching vector not found!"<<std::endl;
    
  } else {
    // parse sticthing vector file
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
          _image_vec.emplace_back(fname, x, y);   

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

    _full_image_width = image_x_max + img_10.x_pos - img_00.x_pos;
    _full_image_height = image_y_max + img_01.y_pos - img_00.y_pos;

  }
}

void OmeTiffCollToZarr::Assemble(const std::string& output_file, VisType v, BS::thread_pool& th_pool)
{

  int num_dims, x_dim, y_dim;

  if (v == VisType::Viv){ //5D file
    x_dim = 4;
    y_dim = 3;
    num_dims = 5;
  
  } else if (v == VisType::TS){ // 3D file
    x_dim = 2;
    y_dim = 1;
    num_dims = 3;
  
  }

  if (_image_vec.size() != 0){
    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto test_source, tensorstore::Open({{"driver", "ometiff"},

                            {"kvstore", {{"driver", "tiled_tiff"},
                                        {"path", _input_dir + "/" + _image_vec[0].file_name}}
                            },
                            },
                            tensorstore::OpenMode::open,
                            tensorstore::ReadWriteMode::read).result());

    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                        ChooseBaseDType(test_source.dtype()));
    std::vector<std::int64_t> new_image_shape(num_dims,1);
    std::vector<std::int64_t> chunk_shape(num_dims,1);
    new_image_shape[y_dim] = _full_image_height;
    new_image_shape[x_dim] = _full_image_width;
    chunk_shape[y_dim] = _chunk_size;
    chunk_shape[x_dim] = _chunk_size;
    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto dest, tensorstore::Open({{"driver", "zarr"},
                                {"kvstore", {{"driver", "file"},
                                            {"path", output_file}}
                                },
                                {"context", {
                                {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                                {"data_copy_concurrency", {{"limit", 8}}},
                                {"file_io_concurrency", {{"limit", 8}}},
                                }},
                                {"metadata", {
                                            {"zarr_format", 2},
                                            {"shape", new_image_shape},
                                            {"chunks", chunk_shape},
                                            {"dtype", base_zarr_dtype.encoded_dtype},
                                            },
                                }},
                                tensorstore::OpenMode::create |
                                tensorstore::OpenMode::delete_existing,
                                tensorstore::ReadWriteMode::write).result());

    for(const auto& i: _image_vec){

        th_pool.push_task([&dest, i, x_dim, y_dim, this](){

        TENSORSTORE_CHECK_OK_AND_ASSIGN(auto source, tensorstore::Open({{"driver", "ometiff"},
                                    {"kvstore", {{"driver", "tiled_tiff"},
                                                {"path", this->_input_dir + "/" + i.file_name}}
                                    }},
                                    tensorstore::OpenMode::open,
                                    tensorstore::ReadWriteMode::read).result());
        
        auto image_shape = source.domain().shape();
        auto image_width = image_shape[4];
        auto image_height = image_shape[3];

        auto array = tensorstore::AllocateArray({image_height, image_width},tensorstore::c_order,
                                                            tensorstore::value_init, source.dtype());

        // initiate a read
        tensorstore::Read(source | 
                tensorstore::Dims(3).ClosedInterval(0, image_height-1) |
                tensorstore::Dims(4).ClosedInterval(0, image_width-1) ,
                array).value();
        
        tensorstore::Write(array, dest |
            tensorstore::Dims(y_dim).ClosedInterval(i.y_pos,i.y_pos+image_height-1) |
            tensorstore::Dims(x_dim).ClosedInterval(i.x_pos,i.x_pos+image_width-1)).value();  
        });
    }

    th_pool.wait_for_tasks();

  }


}

void OmeTiffCollToZarr::GenerateOmeXML(const std::string& image_name, const std::string& output_file){
    if(_image_vec.size() !=0){
        auto tiff_file_path = _input_dir + "/" + _image_vec[0].file_name;
        TIFF *tiff_ = TIFFOpen(tiff_file_path.c_str(), "r");
        if (tiff_ != nullptr) {
            char* infobuf;
            TIFFGetField(tiff_, TIFFTAG_IMAGEDESCRIPTION , &infobuf);
            pugi::xml_document doc;
            pugi::xml_parse_result result = doc.load_string(infobuf);
            TIFFClose(tiff_);
            if (result){

                doc.child("OME").child("Image").attribute("ID").set_value(image_name.c_str());
                doc.child("OME").child("Image").attribute("Name").set_value(image_name.c_str());
                doc.child("OME").child("Image").child("Pixels").attribute("SizeX").set_value(std::to_string(_full_image_width).c_str());
                doc.child("OME").child("Image").child("Pixels").attribute("SizeY").set_value(std::to_string(_full_image_height).c_str());

                for(pugi::xml_node& annotation : doc.child("OME").child("StructuredAnnotations")){
                    std::string key = annotation.child("Value").child("OriginalMetadata").child("Key").child_value();
                    if (key == "ImageWidth"){
                        annotation.child("Value").child("OriginalMetadata").child("Value").first_child().set_value(std::to_string(_full_image_width).c_str());
                    } else if (key == "ImageLength"){
                        annotation.child("Value").child("OriginalMetadata").child("Value").first_child().set_value(std::to_string(_full_image_height).c_str());
                    }
                }
            }
            doc.save_file(output_file.c_str());
        }
    }

}