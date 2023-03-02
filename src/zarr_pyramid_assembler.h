#pragma once

#include <string>
#include "BS_thread_pool.hpp"

struct ImageSegment{
  std::string file_name;
  std::int64_t x_pos, y_pos;

  ImageSegment(std::string fname, std::int64_t x, std::int64_t y):
    file_name(fname), x_pos(x), y_pos(y) {}
 
  ImageSegment():
    file_name(""), x_pos(0), y_pos(0) {};

};

class ZarrPyramidAssembler{

public:
    ZarrPyramidAssembler(const std::string& input_dir,
                         const std::string& output_file,
                         const std::string& stitching_file):
        _input_dir(input_dir),
        _output_file(output_file),
        _stitching_file(stitching_file),
        _th_pool(4)
        {}
    void CreateBaseZarrImage();

private:
    std::int64_t _base_length, _base_width, chunk_size = 1024;
    int _max_level, _min_level;
    std::string _input_dir, _output_file, _stitching_file;
    BS::thread_pool _th_pool;
};

