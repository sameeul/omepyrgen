#pragma once

enum VisType {Viv, TS};

struct Point {
    std::int64_t x, y;
    Point(int64_t x, int64_t y): 
    x(x), y(y){}
};
