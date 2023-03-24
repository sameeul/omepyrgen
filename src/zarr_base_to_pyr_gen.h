#pragma once

#include <string>
#include "BS_thread_pool.hpp"
#include "utilities.h"

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
    std::int64_t _chunk_size = 1080; // need to retrive this dynamically !

    template<typename T>
    void WriteDownsampledImage(const std::string& input_file, const std::string& input_scale_key, const std::string& output_file, const std::string& output_scale_key,
                                                int resolution, VisType v, BS::thread_pool& th_pool);
};

