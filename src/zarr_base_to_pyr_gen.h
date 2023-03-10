#pragma once

#include <string>
#include "BS_thread_pool.hpp"
#include "utils.h"

class ZarrBaseToPyramidGen{

public:
    ZarrBaseToPyramidGen(   const std::string& input_zarr_dir,
                            const std::string& output_root_dir, 
                            int max_level, int min_level):
        _input_zarr_dir(input_zarr_dir),
        _output_root_dir(output_root_dir),
        _max_level(max_level),
        _min_level(min_level)
        {}
    void CreatePyramidImages(VisType v, BS::thread_pool& th_pool);

private:
    std::string _input_zarr_dir, _output_root_dir;
    int _max_level, _min_level;
    std::int64_t _chunk_size = 1080;

    template<typename T>
    void WriteDownsampledImage(const std::string& base_file, const std::string& downsampled_file, VisType v, BS::thread_pool& th_pool);
};

