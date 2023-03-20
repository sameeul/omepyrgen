#include "zarr_pyramid_assembler.h"
#include "utilities.h"
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
#include <fstream>
#include <iostream>
#include <string>
#include <future>
#include <cmath>
#include <thread>
#include <cstdlib>

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


OmeTiffCollToZarr::OmeTiffCollToZarr(const std::string& input_dir,
                      const std::string& stitching_file):
        _input_dir(input_dir),
        _stitching_file(stitching_file){
  std::regex match_str(
      "file:\\s(\\S+);[\\s|\\S]+position:\\s\\((\\d+),\\s(\\d+)\\);[\\s|\\S]+grid:\\s\\((\\d+),\\s(\\d+)\\);");
  

  std::string line;
  std::smatch pieces_match;

  // find final image size 

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
          auto gx = std::stoi(pieces_match[4].str());
          auto gy = std::stoi(pieces_match[5].str());
          _image_vec.emplace_back(fname, gx, gy);   
        }
      }
    }

    if (_image_vec.size() != 0){
      TENSORSTORE_CHECK_OK_AND_ASSIGN(auto test_source, tensorstore::Open(
                          GetOmeTiffSpecToRead( _input_dir + "/" + _image_vec[0].file_name),
                          tensorstore::OpenMode::open,
                          tensorstore::ReadWriteMode::read).result());

      TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                          ChooseBaseDType(test_source.dtype()));
      auto test_image_shape = test_source.domain().shape();
      _chunk_size_x = test_image_shape[4];
      _chunk_size_y = test_image_shape[3];


      int grid_x_max = 0, grid_y_max = 0;

      for(auto &i: _image_vec){
        i._x_grid > grid_x_max ? grid_x_max = i._x_grid : grid_x_max = grid_x_max;
        i._y_grid > grid_y_max ? grid_y_max = i._x_grid : grid_y_max = grid_y_max;
      }

      _full_image_width = (grid_x_max+1)*_chunk_size_x;
      _full_image_height = (grid_y_max+1)*_chunk_size_y;
    }

  }
}

void OmeTiffCollToZarr::Assemble(const std::string& output_file, const std::string& scale_key, VisType v, BS::thread_pool& th_pool)
{

  auto t1 = std::chrono::high_resolution_clock::now();
  int num_dims, x_dim, y_dim;

  if (v == VisType::Viv){ //5D file
    x_dim = 4;
    y_dim = 3;
    num_dims = 5;
  
  } else if (v == VisType::TS_Zarr){ // 3D file
    x_dim = 2;
    y_dim = 1;
    num_dims = 3;
  
  } else if (v == VisType::TS_PCN ){ // 3D file
    x_dim = 1;
    y_dim = 0;
    num_dims = 3;
  }

  if (_image_vec.size() != 0){
    std::list<tensorstore::WriteFutures> pending_writes;
    size_t write_failed_count = 0;



    std::vector<std::int64_t> new_image_shape(num_dims,1);
    std::vector<std::int64_t> chunk_shape(num_dims,1);
    new_image_shape[y_dim] = _full_image_height;
    new_image_shape[x_dim] = _full_image_width;
    chunk_shape[y_dim] = _chunk_size_y;
    chunk_shape[x_dim] = _chunk_size_x;
    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto test_source, tensorstore::Open(
                    GetOmeTiffSpecToRead( _input_dir + "/" + _image_vec[0].file_name),
                    tensorstore::OpenMode::open,
                    tensorstore::ReadWriteMode::read).result());

    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                        ChooseBaseDType(test_source.dtype()));
    tensorstore::Spec output_spec{};
  
    if (v == VisType::TS_Zarr | v == VisType::Viv){
      output_spec = GetZarrSpecToWrite(output_file + "/" + scale_key, new_image_shape, chunk_shape, base_zarr_dtype.encoded_dtype);
    } else if (v == VisType::TS_PCN){
      output_spec = GetPCNSpecToWrite(output_file, scale_key, new_image_shape, chunk_shape, test_source.dtype().name(), true);
    }

    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto dest, tensorstore::Open(
                                output_spec,
                                tensorstore::OpenMode::create |
                                tensorstore::OpenMode::delete_existing,
                                tensorstore::ReadWriteMode::write).result());
    
    for(const auto& i: _image_vec){
        
        th_pool.push_task([&dest, i, x_dim, y_dim, this, &pending_writes, v](){

        TENSORSTORE_CHECK_OK_AND_ASSIGN(auto source, tensorstore::Open(
                                    GetOmeTiffSpecToRead(this->_input_dir + "/" + i.file_name),
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

        tensorstore::IndexTransform<> transform = tensorstore::IdentityTransform(dest.domain());
        if(v == VisType::TS_PCN){
          transform = (std::move(transform) | tensorstore::Dims(2, 3).IndexSlice({0,0})).value();

        } 
        transform = (std::move(transform) | tensorstore::Dims(y_dim).ClosedInterval(i._y_grid*this->_chunk_size_y,i._y_grid*this->_chunk_size_y+image_height-1) 
                                            | tensorstore::Dims(x_dim).ClosedInterval(i._x_grid*this->_chunk_size_x,i._x_grid*this->_chunk_size_x+image_width-1)).value(); 
        pending_writes.emplace_back(tensorstore::Write(array, dest | transform));

        });
    }

    th_pool.wait_for_tasks();

    size_t total_writes = pending_writes.size(); 

    size_t count = 0, num_pass = 10;
    while( num_pass > 0){
      for (auto it = pending_writes.begin(); it != pending_writes.end();) {
        if (it->commit_future.ready()) {
          if (!GetStatus(it->commit_future).ok()) {
            write_failed_count++;
            std::cout << GetStatus(it->commit_future);
          }
          count++;
          it = pending_writes.erase(it);
        } else {
          ++it;
        }
      }
      --num_pass;
    }

    std::cout << "remaining stuff "<< pending_writes.size() << std::endl; 
    // force the rest of it to finish
    for (auto& front : pending_writes) {
      if (!GetStatus(front.commit_future).ok()) {
        write_failed_count++;
      }
    }
  }

  auto t2 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> et1 = t2-t1;
  std::cout << "time for assembly: "<< et1.count() << std::endl;


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