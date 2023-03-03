#include "ome_tiff_to_zarr_pyramid.h"
#include <filesystem>
namespace fs = std::filesystem;

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

    std::string tiff_file_name = fs::path(_input_file).stem().string();
    std::string output_file_loc = _output_dir + "/" + tiff_file_name + ".zarr";
    if (v == VisType::Viv){
        output_file_loc = output_file_loc + "/data.zarr/0";
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

void OmeTifftoZarrPyramid::WriteMultiscaleMetadata(VisType v)
{
  // json zarr_multiscale_axes;
  // zarr_multiscale_axes = json::parse(R"([
  //               {"name": "z", "type": "space", "unit": "micrometer"},
  //               {"name": "y", "type": "space", "unit": "micrometer"},
  //               {"name": "x", "type": "space", "unit": "micrometer"}
  //           ])");
  
 
  // float level = 1.0;
  // json scale_metadata_list = json::array();
  // for(int i=_max_level; i>=_min_level; i--){
  //   json scale_metadata;
  //   scale_metadata["path"] = std::to_string(i);
  //   scale_metadata["coordinateTransformations"] = {{{"type", "scale"}, {"scale", {1.0, level, level}}}};
  //   scale_metadata_list.push_back(scale_metadata);
  //   level = level*2;
  // }

  // json combined_metadata;
  // combined_metadata["datasets"] = scale_metadata_list;
  // combined_metadata["version"] = "0.4";
  // combined_metadata["axes"] = zarr_multiscale_axes;
  // combined_metadata["name"] = fs::path(_input_file).filename();
  // combined_metadata["metadata"] = {{"method", "mean"}};
  // json final_formated_metadata;
  // final_formated_metadata["multiscales"] = {combined_metadata};
  // std::ofstream f(_output_file + "/.zattrs",std::ios_base::trunc |std::ios_base::out);
  // f << final_formated_metadata;

}