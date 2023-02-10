#include <string>

struct Chunk {
    std::int64_t x1, x2, y1, y2;
    Chunk(int64_t x1, int64_t x2, int64_t y1, int64_t y2): 
    x1(x1), x2(x2), y1(y1), y2(y2){}
};

class ZarrPyramidWriter{

public:
    ZarrPyramidWriter(std::string& input_file, std::string& output_file):
        _input_file(input_file),
        _output_file(output_file)
        {}
    void CreateBaseZarrImage();
    void CreateBaseZarrImageV2();
    void CreatePyramidImages();
    void WriteMultiscaleMetadata();

private:
    std::int64_t _base_length, _base_width;
    int _max_level, _min_level;
    std::string _input_file, _output_file;
};
