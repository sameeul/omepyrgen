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


    for (int i=_max_level; i>_min_level; --i){
        std::string input_path;
        if (i== _max_level){
            input_path = _input_zarr_dir;
        } else {
            input_path = _output_root_dir + "/" + std::to_string(i) + "/";
        }
        std::string output_path = _output_root_dir + "/" + std::to_string(i-1) + "/";
        WriteDownsampledImage<uint16_t>(input_path, output_path, v, th_pool);
    } 
}

template <typename T>
void ZarrBaseToPyramidGen::WriteDownsampledImage(const std::string& base_file, const std::string& downsampled_file,
                                                VisType v, BS::thread_pool& th_pool)
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
    
    }

    //tensorstore::Context context = Context::Default();
    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store1, tensorstore::Open(
                            GetZarrSpecToRead(base_file),
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


    TENSORSTORE_CHECK_OK_AND_ASSIGN(auto store2, tensorstore::Open(
                            GetZarrSpecToWrite(downsampled_file, new_image_shape, chunk_shape, base_zarr_dtype.encoded_dtype),
                            tensorstore::OpenMode::create |
                            tensorstore::OpenMode::delete_existing,
                            tensorstore::ReadWriteMode::write).result());
    std::list<tensorstore::WriteFutures> pending_writes;
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

            th_pool.push_task([store1, store2, prev_tl, prev_br, cur_tl, cur_br, x_dim, y_dim, &pending_writes](){  
                std::vector<T> read_buffer((prev_br.x-prev_tl.x)*(prev_br.y-prev_tl.y));
                auto array = tensorstore::Array(read_buffer.data(), {prev_br.y-prev_tl.y, prev_br.x-prev_tl.x}, tensorstore::c_order);

                tensorstore::Read(store1 |
                        tensorstore::Dims(y_dim).ClosedInterval(prev_tl.y, prev_br.y-1) |
                        tensorstore::Dims(x_dim).ClosedInterval(prev_tl.x, prev_br.x-1) ,
                        tensorstore::UnownedToShared(array)).value();

                auto result = DownsampleAverage(read_buffer, (prev_br.y-prev_tl.y), (prev_br.x-prev_tl.x));
                auto result_array = tensorstore::Array(result->data(), {cur_br.y-cur_tl.y, cur_br.x-cur_tl.x}, tensorstore::c_order);
                pending_writes.emplace_back(tensorstore::Write(tensorstore::UnownedToShared(result_array), store2 | 
                    tensorstore::Dims(y_dim).ClosedInterval(cur_tl.y, cur_br.y-1) |
                    tensorstore::Dims(x_dim).ClosedInterval(cur_tl.x, cur_br.x-1)));     

            }); 
        }
    }
    th_pool.wait_for_tasks();
    size_t total_writes = pending_writes.size(); 

    size_t count = 0, num_pass = 10;
    size_t write_failed_count = 0;
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