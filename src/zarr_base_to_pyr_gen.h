#pragma once

#include <string>
#include "BS_thread_pool.hpp"
#include "utilities.h"

class ChunkedBaseToPyramid{

public:
    ChunkedBaseToPyramid(){}
    void CreatePyramidImages(   const std::string& input_zarr_dir,
                                const std::string& output_root_dir, 
                                int base_scale_key,
                                int min_dim, 
                                VisType v, 
                                DSType ds,
                                BS::thread_pool& th_pool);

private:
    template<typename T>
    void WriteDownsampledImage( const std::string& input_file, const std::string& input_scale_key, 
                                const std::string& output_file, const std::string& output_scale_key,
                                int resolution, VisType v, DSType ds, BS::thread_pool& th_pool);
};

