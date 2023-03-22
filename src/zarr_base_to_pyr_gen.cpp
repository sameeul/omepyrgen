#include "zarr_base_to_pyr_gen.h"
#include "downsample.h"
#include "utilities.h"

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

void ZarrBaseToPyramidGen::CreatePyramidImages(VisType v, BS::thread_pool& th_pool)
{

    int resolution = 1; // this gets doubled in each level up
    for (int i=_max_level; i>_min_level; --i){
        resolution *= 2;
        WriteDownsampledImage<uint16_t>(_input_zarr_dir, std::to_string(i), _output_root_dir, std::to_string(i-1), resolution, v, th_pool);
    } 
}

template <typename T>
void ZarrBaseToPyramidGen::WriteDownsampledImage(   const std::string& input_file, const std::string& input_scale_key, 
                                                    const std::string& output_file, const std::string& output_scale_key,
                                                    int resolution, VisType v, BS::thread_pool& th_pool)
{
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
        x_dim = 1;
        y_dim = 0;
        num_dims = 3;
    }
    tensorstore::Spec input_spec{};
    if (v == VisType::TS_Zarr | v == VisType::Viv){
      input_spec = GetZarrSpecToRead(input_file, input_scale_key);
    } else if (v == VisType::TS_NPC){
      input_spec = GetNPCSpecToRead(input_file, input_scale_key);
    }
    //tensorstore::Context context = Context::Default();
    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store1, tensorstore::Open(
                            input_spec,
                            tensorstore::OpenMode::open,
                            tensorstore::ReadWriteMode::read).result());
    auto prev_image_shape = store1.domain().shape();
    //auto chunk_shape = store1.chunk_layout().read_chunk_shape();

    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto base_zarr_dtype,
                                        ChooseBaseDType(store1.dtype()));
    auto prev_x_max = static_cast<std::int64_t>(prev_image_shape[x_dim]);
    auto prev_y_max = static_cast<std::int64_t>(prev_image_shape[y_dim]);

    auto cur_x_max = static_cast<std::int64_t>(ceil(prev_x_max/2.0));
    auto cur_y_max = static_cast<std::int64_t>(ceil(prev_y_max/2.0));

    auto num_rows = static_cast<std::int64_t>(ceil(1.0*cur_y_max/_chunk_size));
    auto num_cols = static_cast<std::int64_t>(ceil(1.0*cur_x_max/_chunk_size));



    std::vector<std::int64_t> new_image_shape(num_dims,1);
    std::vector<std::int64_t> chunk_shape(num_dims,1);
    new_image_shape[y_dim] = cur_y_max;
    new_image_shape[x_dim] = cur_x_max;
    chunk_shape[y_dim] = _chunk_size;
    chunk_shape[x_dim] = _chunk_size;

    tensorstore::Spec output_spec{};
    auto open_mode = tensorstore::OpenMode::create;

    if (v == VisType::TS_Zarr | v == VisType::Viv){
      output_spec = GetZarrSpecToWrite(output_file + "/" + output_scale_key, new_image_shape, chunk_shape, base_zarr_dtype.encoded_dtype);
      open_mode = open_mode | tensorstore::OpenMode::delete_existing;
    } else if (v == VisType::TS_NPC){
      output_spec = GetNPCSpecToWrite(output_file, output_scale_key, new_image_shape, chunk_shape, resolution, store1.dtype().name(), false);
    }
    
    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store2, tensorstore::Open(
                            output_spec,
                            open_mode,
                            tensorstore::ReadWriteMode::write).result());
    for(std::int64_t i=0; i<num_rows; ++i){
        auto y_start = i*_chunk_size;
        auto y_end = std::min({(i+1)*_chunk_size, cur_y_max});

        auto prev_y_start = 2*y_start;
        auto prev_y_end = std::min({2*y_end, prev_y_max});
        for(std::int64_t j=0; j<num_cols; ++j){
            auto x_start = j*_chunk_size;
            auto x_end = std::min({(j+1)*_chunk_size, cur_x_max});
            auto prev_x_start = 2*x_start;
            auto prev_x_end = std::min({2*x_end, prev_x_max});
            
            /*
                br -> Bottom Right
                tl -< Top Left

                these two points bounds a rectengular chunk
            */

            Point prev_tl(prev_x_start, prev_y_start);
            Point prev_br(prev_x_end, prev_y_end);

            Point cur_tl(x_start, y_start);
            Point cur_br(x_end, y_end);
            
            th_pool.push_task([ &store1, &store2, 
                                prev_x_start, prev_x_end, prev_y_start, prev_y_end, 
                                x_start, x_end, y_start, y_end, 
                                x_dim, y_dim, v](){  
                std::vector<T> read_buffer((prev_x_end-prev_x_start)*(prev_y_end-prev_y_start));
                auto array = tensorstore::Array(read_buffer.data(), {prev_y_end-prev_y_start, prev_x_end-prev_x_start}, tensorstore::c_order);

                tensorstore::IndexTransform<> input_transform = tensorstore::IdentityTransform(store1.domain());
                if(v == VisType::TS_NPC){
                input_transform = (std::move(input_transform) | tensorstore::Dims(2, 3).IndexSlice({0,0})).value();

                } 
                input_transform = (std::move(input_transform) | tensorstore::Dims(y_dim).ClosedInterval(prev_y_start, prev_y_end-1) 
                                                    | tensorstore::Dims(x_dim).ClosedInterval(prev_x_start, prev_x_end-1)).value(); 

                tensorstore::Read(store1 | input_transform, tensorstore::UnownedToShared(array)).value();

                auto result = DownsampleAverage(read_buffer, (prev_y_end-prev_y_start), (prev_x_end-prev_x_start));
                auto result_array = tensorstore::Array(result->data(), {y_end-y_start, x_end-x_start}, tensorstore::c_order);

                tensorstore::IndexTransform<> output_transform = tensorstore::IdentityTransform(store2.domain());
                if(v == VisType::TS_NPC){
                output_transform = (std::move(output_transform) | tensorstore::Dims(2, 3).IndexSlice({0,0})).value();

                } 
                output_transform = (std::move(output_transform) | tensorstore::Dims(y_dim).ClosedInterval(y_start, y_end-1) 
                                                                | tensorstore::Dims(x_dim).ClosedInterval(x_start, x_end-1)).value(); 

                tensorstore::Write(tensorstore::UnownedToShared(result_array), store2 | output_transform).value();  
            }); 
        }
    }
    th_pool.wait_for_tasks();
}