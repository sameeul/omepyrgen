#pragma once

#include <string>
#include "BS_thread_pool.hpp"

struct Chunk {
    std::int64_t x1, x2, y1, y2;
    Chunk(int64_t x1, int64_t x2, int64_t y1, int64_t y2): 
    x1(x1), x2(x2), y1(y1), y2(y2){}
};

class ZarrPyramidWriter{

public:
    ZarrPyramidWriter(std::string& input_file, std::string& output_file):
        _input_file(input_file),
        _output_file(output_file),
        _th_pool(4)
        {}
    void CreateBaseZarrImage();
    void CreateBaseZarrImageV2();
    void CreatePyramidImages();
    void WriteMultiscaleMetadata();
    template<typename T>
    void WriteDownsampledImage(std::string& base_file, std::string& downsampled_file);
private:
    std::int64_t _base_length, _base_width, chunk_size = 1024;
    int _max_level, _min_level;
    std::string _input_file, _output_file;
    BS::thread_pool _th_pool;
};

