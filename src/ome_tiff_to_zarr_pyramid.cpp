#include "ome_tiff_to_zarr_pyramid.h"


OmeTifftoZarrPyramid::OmeTifftoZarrPyramid( const std::string& input_file, 
                                            const std::string& output_dir,
                                            int min_dim):
    _input_file(input_file),
    _output_dir(output_dir){

    TIFF *tiff_ = TIFFOpen(input_file.c_str(), "r");
    if (tiff_ != nullptr) {
        uint32_t image_width = 0, image_height = 0;
        TIFFGetField(tiff_, TIFFTAG_IMAGEWIDTH, &image_width);
        TIFFGetField(tiff_, TIFFTAG_IMAGELENGTH, &image_height);
        _max_level = static_cast<int>(ceil(log2(std::max({image_width, image_height}))));
        _min_level = static_cast<int>(ceil(log2(min_dim)));
        TIFFClose(tiff_);     
    }

}   

 
void OmeTifftoZarrPyramid::Generate(VisType v){
    std::string output_file_loc = _output_dir + "/" + _input_file + "_zarr";
    if (v == VisType::Viv){
        output_file_loc = output_file_loc + "data.zarr" + "/0";
    }

    std::string base_zarr_file = output_file_loc + "/" + std::to_string(_max_level);
    _zpw_ptr = std::make_unique<OmeTiffToZarrConverter>();
    _zpg_ptr = std::make_unique<ZarrBaseToPyramidGen>(base_zarr_file, output_file_loc,  
                                _max_level, _min_level);
    std::cout << "writing base zarr image"<<std::endl;
    _zpw_ptr->Convert(_input_file, base_zarr_file, v, _th_pool);
    std::cout << "generating image pyramids"<<std::endl;
    _zpg_ptr->CreatePyramidImages(v, _th_pool);
}

