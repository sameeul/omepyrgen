#pragma once

#include <string>
#include "BS_thread_pool.hpp"
#include "utils.h"

class OmeTiffToZarrConverter{

public:
    OmeTiffToZarrConverter(){}
    void Convert(   const std::string& input_file, 
                    const std::string& output_file, 
                    VisType v,
                    BS::thread_pool& th_pool
                );

    void WriteMultiscaleMetadata();


private:
    std::int64_t _image_length, _image_width, _chunk_size = 1024;
};

