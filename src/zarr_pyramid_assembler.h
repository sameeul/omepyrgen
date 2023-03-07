#pragma once

#include <string>
#include "utils.h"
#include "BS_thread_pool.hpp"

struct ImageSegment{
  std::string file_name;
  std::int64_t x_pos, y_pos;

  ImageSegment(std::string fname, std::int64_t x, std::int64_t y):
    file_name(fname), x_pos(x), y_pos(y) {}
 
  ImageSegment():
    file_name(""), x_pos(0), y_pos(0) {};

};

class OmeTiffCollToZarr{

public:
    OmeTiffCollToZarr(const std::string& input_dir,
                      const std::string& stitching_file);

    void Assemble(const std::string& output_file, VisType v, BS::thread_pool& th_pool);
    std::int64_t image_height() {return _full_image_height;}
    std::int64_t image_width() {return _full_image_width;}
    void GenerateOmeXML(const std::string& image_name, const std::string& output_file);

private:
    std::int64_t _full_image_height, _full_image_width, _chunk_size = 1024;
    std::string _input_dir, _stitching_file;
    std::vector<ImageSegment> _image_vec;
};

