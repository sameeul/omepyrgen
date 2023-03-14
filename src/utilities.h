#pragma once
#include <string>
#include <memory>
#include <vector>
#include<cmath>

#include "tensorstore/tensorstore.h"
#include "tensorstore/spec.h"

enum VisType {Viv, TS_Zarr, TS_PCN};

struct Point {
    std::int64_t x, y;
    Point(std::int64_t x, std::int64_t y): 
    x(x), y(y){}
};

tensorstore::Spec GetOmeTiffSpecToRead(const std::string& filename);
tensorstore::Spec GetZarrSpecToRead(const std::string& filename);
tensorstore::Spec GetZarrSpecToWrite(   const std::string& filename, 
                                        std::vector<std::int64_t>& image_shape, 
                                        std::vector<std::int64_t>& chunk_shape,
                                        std::string& dtype);

