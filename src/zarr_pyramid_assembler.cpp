#include "zarr_pyramid_assembler.h"
#include "utilities.h"
#include "pugixml.hpp"
#include "tiffio.h"
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>
#include <list>
#include <cstring>
#include <unordered_set>
#include <string>
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



void OmeTiffCollToChunked::Assemble(const std::string& input_dir,
                                    const std::string& stitching_file,
                                    const std::string& output_file, 
                                    const std::string& scale_key, 
                                    VisType v, 
                                    BS::thread_pool& th_pool)
{
  std::regex match_str(
      "file:\\s(\\S+);[\\s|\\S]+position:\\s\\((\\d+),\\s(\\d+)\\,\\s(\\d+)\\);[\\s|\\S]+grid:\\s\\((\\d+),\\s(\\d+)\\);");
  

  std::string line;
  std::smatch pieces_match;
  int grid_x_max = 0, grid_y_max = 0, grid_c_max = 0;
  std::vector<ImageSegment> image_vec;
  _chunk_size_x = 0;
  _chunk_size_y = 0;
  _full_image_width = 0;
  _full_image_height = 0;

  std::fstream stitch_vec;
  stitch_vec.open(stitching_file, std::ios::in);
  if(!stitch_vec.is_open()){
    std::cout<<"Stitching vector not found!"<<std::endl;
    
  } else {
    // parse sticthing vector file
    while (getline(stitch_vec, line)) {
      if (std::regex_match(line, pieces_match, match_str)) {
        if(pieces_match.size() == 7){
          auto fname = pieces_match[1].str();
          auto gc = std::stoi(pieces_match[4].str());
          auto gx = std::stoi(pieces_match[5].str());
          auto gy = std::stoi(pieces_match[6].str());
          if (gx < 10 && gy < 10){
            gc > grid_c_max ? grid_c_max = gc : grid_c_max = grid_c_max;
            gx > grid_x_max ? grid_x_max = gx : grid_x_max = grid_x_max;
            gy > grid_y_max ? grid_y_max = gy : grid_y_max = grid_y_max;
            image_vec.emplace_back(fname, gx, gy, gc);   
          }
        }
      }
    }
  }
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
  
  } else if (v == VisType::TS_NPC ){ // 3D file
    x_dim = 0;
    y_dim = 1;
    num_dims = 3;
  }

  if (image_vec.size() != 0){
    std::list<tensorstore::WriteFutures> pending_writes;
    size_t write_failed_count = 0;
    std::string sample_tiff_file = input_dir + "/" + image_vec[0].file_name;
    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto test_source, tensorstore::Open(
                    GetOmeTiffSpecToRead(sample_tiff_file),
                    tensorstore::OpenMode::open,
                    tensorstore::ReadWriteMode::read).result());
    auto test_image_shape = test_source.domain().shape();
    _chunk_size_x = test_image_shape[4];
    _chunk_size_y = test_image_shape[3];
    _full_image_width = (grid_x_max+1)*_chunk_size_x;
    _full_image_height = (grid_y_max+1)*_chunk_size_y;
    _num_channels = grid_c_max+1;

    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                        ChooseBaseDType(test_source.dtype()));
    
    tensorstore::Spec output_spec{};
    
    std::vector<std::int64_t> new_image_shape(num_dims,1);
    std::vector<std::int64_t> chunk_shape(num_dims,1);
    new_image_shape[y_dim] = _full_image_height;
    new_image_shape[x_dim] = _full_image_width;
    chunk_shape[y_dim] = _chunk_size_y;
    chunk_shape[x_dim] = _chunk_size_x;

    if (v == VisType::TS_Zarr ){
      output_spec = GetZarrSpecToWrite(output_file + "/" + scale_key, new_image_shape, chunk_shape, base_zarr_dtype.encoded_dtype);
    } else if (v == VisType::Viv){
      new_image_shape[1] = _num_channels;
      
      output_spec = GetZarrSpecToWrite(output_file + "/" + scale_key, new_image_shape, chunk_shape, base_zarr_dtype.encoded_dtype);
    }  else if (v == VisType::TS_NPC){
      output_spec = GetNPCSpecToWrite(output_file, scale_key, new_image_shape, chunk_shape, 1, _num_channels, test_source.dtype().name(), true);
    }

    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto dest, tensorstore::Open(
                                output_spec,
                                tensorstore::OpenMode::create |
                                tensorstore::OpenMode::delete_existing,
                                tensorstore::ReadWriteMode::write).result());
    
    std::cout <<"shape is " << dest.domain().shape() << std::endl;
    auto t4 = std::chrono::high_resolution_clock::now();
    for(const auto& i: image_vec){        
      th_pool.push_task([&dest, &input_dir, i, x_dim, y_dim, this, &pending_writes, v](){


        TENSORSTORE_CHECK_OK_AND_ASSIGN(auto source, tensorstore::Open(
                                    GetOmeTiffSpecToRead(input_dir + "/" + i.file_name),
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
        if(v == VisType::TS_NPC){
          transform = (std::move(transform) | tensorstore::Dims("z", "channel").IndexSlice({0, i._c_grid}) 
                                            | tensorstore::Dims(y_dim).SizedInterval(i._y_grid*this->_chunk_size_y, image_height) 
                                            | tensorstore::Dims(x_dim).SizedInterval(i._x_grid*this->_chunk_size_x, image_width)
                                            | tensorstore::Dims(x_dim, y_dim).Transpose({y_dim, x_dim})).value();

        } else if (v == VisType::TS_Zarr){
          transform = (std::move(transform) | tensorstore::Dims(y_dim).SizedInterval(i._y_grid*this->_chunk_size_y, image_height) 
                                            | tensorstore::Dims(x_dim).SizedInterval(i._x_grid*this->_chunk_size_x, image_width)).value();
        } else if (v == VisType::Viv){
          transform = (std::move(transform) | tensorstore::Dims(1).SizedInterval(i._c_grid, 1) 
                                            | tensorstore::Dims(y_dim).SizedInterval(i._y_grid*this->_chunk_size_y, image_height) 
                                            | tensorstore::Dims(x_dim).SizedInterval(i._x_grid*this->_chunk_size_x, image_width)).value();
        }

        pending_writes.emplace_back(tensorstore::Write(array, dest | transform));
      });
    }

    th_pool.wait_for_tasks();

    size_t total_writes = pending_writes.size(); 

    size_t count = 0, num_pass = 10;
    while( num_pass > 0){
      for (auto it = pending_writes.begin(); it != pending_writes.end();) {
        if (it->commit_future.ready()) {
          if (!tensorstore::GetStatus(it->commit_future).ok()) {
            write_failed_count++;
            std::cout << tensorstore::GetStatus(it->commit_future);
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
      if (!tensorstore::GetStatus(front.commit_future).ok()) {
        write_failed_count++;
      }
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et2 = t3-t4;
    std::cout << "time for assembly: "<< et2.count() << std::endl;



  }

  auto t2 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> et1 = t2-t1;
  std::cout << "time for assembly: "<< et1.count() << std::endl;


}

void OmeTiffCollToChunked::GenerateOmeXML(const std::string& image_name, const std::string& output_file){

    pugi::xml_document doc;

    // Create the root element <OME>
    pugi::xml_node omeNode = doc.append_child("OME");
    
    // Add the namespaces and attributes to the root element
    omeNode.append_attribute("xmlns") = "http://www.openmicroscopy.org/Schemas/OME/2016-06";
    omeNode.append_attribute("xmlns:xsi") = "http://www.w3.org/2001/XMLSchema-instance";
    omeNode.append_attribute("Creator") = "OmePyrGen 0.0.1";
    omeNode.append_attribute("UUID") = "urn:uuid:ce3367ae-0512-4e87-a045-20d87db14001";
    omeNode.append_attribute("xsi:schemaLocation") = "http://www.openmicroscopy.org/Schemas/OME/2016-06 http://www.openmicroscopy.org/Schemas/OME/2016-06/ome.xsd";

    // Create the <Image> element
    pugi::xml_node imageNode = omeNode.append_child("Image");
    imageNode.append_attribute("ID") = image_name.c_str();;
    imageNode.append_attribute("Name") =image_name.c_str();

    // Create the <Pixels> element
    pugi::xml_node pixelsNode = imageNode.append_child("Pixels");
    pixelsNode.append_attribute("BigEndian") = "false";
    pixelsNode.append_attribute("DimensionOrder") = "XYZCT";
    pixelsNode.append_attribute("ID") = "Pixels:0";
    pixelsNode.append_attribute("Interleaved") = "false";
    pixelsNode.append_attribute("SizeC") = "1";
    pixelsNode.append_attribute("SizeT") = "1";
    pixelsNode.append_attribute("SizeX") = std::to_string(_full_image_width).c_str();
    pixelsNode.append_attribute("SizeY") = std::to_string(_full_image_height).c_str();
    pixelsNode.append_attribute("SizeZ") = "1";
    pixelsNode.append_attribute("Type") = "uint8";

    // Create the <Channel> element
    pugi::xml_node channelNode = pixelsNode.append_child("Channel");
    channelNode.append_attribute("ID") = "Channel:0:0";
    channelNode.append_attribute("SamplesPerPixel") = "1";

    // Create the <LightPath> element
    channelNode.append_child("LightPath");

    doc.save_file(output_file.c_str());
}