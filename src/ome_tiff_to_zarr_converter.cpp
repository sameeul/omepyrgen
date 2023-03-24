#include "ome_tiff_to_zarr_converter.h"
#include "utilities.h"

#include <string>
#include <unistd.h>
#include <stdint.h>
#include <iostream>
#include <string>
#include <string_view>
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

using ::tensorstore::Context;
using ::tensorstore::internal_zarr::ChooseBaseDType;

void OmeTiffToZarrConverter::Convert( const std::string& input_file, const std::string& output_file, 
                                      const std::string& scale_key, const VisType v, BS::thread_pool& th_pool){
  
  int num_dims, x_dim, y_dim;

  if (v == VisType::Viv){ //5D file
    x_dim = 4;
    y_dim = 3;
    num_dims = 5;
  } else if (v == VisType::TS_Zarr ){ // 3D file
    x_dim = 2;
    y_dim = 1;
    num_dims = 3;
  } else if (v == VisType::TS_NPC ){ // 3D file
    x_dim = 0;
    y_dim = 1;
    num_dims = 3;
  }

  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store1, tensorstore::Open(GetOmeTiffSpecToRead(input_file),
                            tensorstore::OpenMode::open,
                            tensorstore::ReadWriteMode::read).result());

  auto shape = store1.domain().shape();
  auto data_type = GetDataTypeCode(store1.dtype().name()); 
  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                     ChooseBaseDType(store1.dtype()));
  _image_length = shape[3]; // as per tiled_tiff spec
  _image_width = shape[4];
  std::vector<std::int64_t> new_image_shape(num_dims,1);
  std::vector<std::int64_t> chunk_shape(num_dims,1);
  new_image_shape[y_dim] = _image_length;
  new_image_shape[x_dim] = _image_width;
  chunk_shape[y_dim] = _chunk_size;
  chunk_shape[x_dim] = _chunk_size;

  auto num_rows = static_cast<std::int64_t>(ceil(1.0*_image_length/_chunk_size));
  auto num_cols = static_cast<std::int64_t>(ceil(1.0*_image_width/_chunk_size));

  tensorstore::Spec output_spec{};
  
  if (v == VisType::TS_Zarr | v == VisType::Viv){
    output_spec = GetZarrSpecToWrite(output_file + "/" + scale_key, new_image_shape, chunk_shape, base_zarr_dtype.encoded_dtype);
  } else if (v == VisType::TS_NPC){
    output_spec = GetNPCSpecToWrite(output_file, scale_key, new_image_shape, chunk_shape, 1, store1.dtype().name(), true);
  }

  TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store2, tensorstore::Open(
                            output_spec,
                            tensorstore::OpenMode::create |
                            tensorstore::OpenMode::delete_existing,
                            tensorstore::ReadWriteMode::write).result());

  for(std::int64_t i=0; i<num_rows; ++i){
    std::int64_t y_start = i*_chunk_size;
    std::int64_t y_end = std::min({(i+1)*_chunk_size, _image_length});
    for(std::int64_t j=0; j<num_cols; ++j){
      std::int64_t x_start = j*_chunk_size;
      std::int64_t x_end = std::min({(j+1)*_chunk_size, _image_width});
      th_pool.push_task([&store1, &store2, x_start, x_end, y_start, y_end, x_dim, y_dim, v](){  

        auto array = tensorstore::AllocateArray({y_end-y_start, x_end-x_start},tensorstore::c_order,
                                tensorstore::value_init, store1.dtype());
        // initiate a read
        tensorstore::Read(store1 | 
                          tensorstore::Dims(3).ClosedInterval(y_start,y_end-1) |
                          tensorstore::Dims(4).ClosedInterval(x_start,x_end-1) ,
                          array).value();
        
        tensorstore::IndexTransform<> transform = tensorstore::IdentityTransform(store2.domain());
        if(v == VisType::TS_NPC){
          transform = (std::move(transform) | tensorstore::Dims(2, 3).IndexSlice({0,0}) 
                                            | tensorstore::Dims(y_dim).ClosedInterval(y_start,y_end-1) 
                                            | tensorstore::Dims(x_dim).ClosedInterval(x_start,x_end-1)
                                            | tensorstore::Dims(x_dim, y_dim).Transpose({y_dim, x_dim})).value();
        }else if (v == VisType::TS_Zarr || v == VisType::Viv){
          transform = (std::move(transform) | tensorstore::Dims(y_dim).ClosedInterval(y_start,y_end-1) 
                                            | tensorstore::Dims(x_dim).ClosedInterval(x_start,x_end-1)).value();
        }
        tensorstore::Write(array, store2 | transform).value();
      });       

    }
  }
  th_pool.wait_for_tasks();
}


