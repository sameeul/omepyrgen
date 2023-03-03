#include "ome_tiff_to_zarr_pyramid.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

OmeTifftoZarrPyramid::OmeTifftoZarrPyramid( const std::string& input_file, 
                                            const std::string& output_dir,
                                            int min_dim):
    _input_file(input_file),
    _output_dir(output_dir){

    TIFF *tiff_ = TIFFOpen(_input_file.c_str(), "r");
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
    WriteMultiscaleMetadata(v);
}

void OmeTifftoZarrPyramid::WriteMultiscaleMetadata(VisType v)
{
    if(v == VisType::TS){
        WriteTSZattrFile();
    } else if (v == VisType::Viv){
        ExtractAndWriteXML();
        WriteVivZattrFile();
        WriteVivZgroupFiles();
    }
    

}

void OmeTifftoZarrPyramid::WriteVivZgroupFiles(){
    std::string tiff_file_name = fs::path(_input_file).stem().string();
    std::string output_file_loc_1 = _output_dir + "/" + tiff_file_name+".zarr/data.zarr/.zgroup";
    std::string zgroup_text = "{\"zarr_format\": 2}";
    std::ofstream zgroup_file_1( output_file_loc_1, std::ios_base::out );
    if(zgroup_file_1.is_open()){
        zgroup_file_1 << zgroup_text << std::endl;
    }
    std::string output_file_loc_2 = _output_dir + "/" + tiff_file_name+".zarr/data.zarr/0/.zgroup";
    std::ofstream zgroup_file_2( output_file_loc_2, std::ios_base::out );
    if(zgroup_file_2.is_open()){
        zgroup_file_2 << zgroup_text << std::endl;
    }
}

void OmeTifftoZarrPyramid::ExtractAndWriteXML(){
    std::string tiff_file_name = fs::path(_input_file).stem().string();
    std::string output_file_loc = _output_dir + "/" + tiff_file_name+".zarr";
    TIFF *tiff_ = TIFFOpen(_input_file.c_str(), "r");
    if (tiff_ != nullptr) {
        char* infobuf;
        TIFFGetField(tiff_, TIFFTAG_IMAGEDESCRIPTION , &infobuf);
        char* new_pos = strstr(infobuf, "<OME");
        std::ofstream metadata_file( output_file_loc+"/METADATA.ome.xml", std::ios_base::out );
        if(metadata_file.is_open()){
            metadata_file << new_pos << std::endl;
        }
        if(!metadata_file){
            std::cout<<"Unable to write metadafile"<<std::endl;
        }
        TIFFClose(tiff_);
    }
}

void OmeTifftoZarrPyramid::WriteTSZattrFile(){
    std::string tiff_file_name = fs::path(_input_file).stem().string();
    std::string output_file_loc = _output_dir + "/" + tiff_file_name + ".zarr";
    json zarr_multiscale_axes;
    zarr_multiscale_axes = json::parse(R"([
                    {"name": "z", "type": "space", "unit": "micrometer"},
                    {"name": "y", "type": "space", "unit": "micrometer"},
                    {"name": "x", "type": "space", "unit": "micrometer"}
                ])");
    
    
    float level = 1.0;
    json scale_metadata_list = json::array();
    for(int i=_max_level; i>=_min_level; i--){
        json scale_metadata;
        scale_metadata["path"] = std::to_string(i);
        scale_metadata["coordinateTransformations"] = {{{"type", "scale"}, {"scale", {1.0, level, level}}}};
        scale_metadata_list.push_back(scale_metadata);
        level = level*2;
    }

    json combined_metadata;
    combined_metadata["datasets"] = scale_metadata_list;
    combined_metadata["version"] = "0.4";
    combined_metadata["axes"] = zarr_multiscale_axes;
    combined_metadata["name"] = fs::path(_input_file).filename();
    combined_metadata["metadata"] = {{"method", "mean"}};
    json final_formated_metadata;
    final_formated_metadata["multiscales"] = {combined_metadata};
    std::ofstream f(output_file_loc + "/.zattrs",std::ios_base::trunc |std::ios_base::out);
    f << final_formated_metadata;
}

void OmeTifftoZarrPyramid::WriteVivZattrFile(){
    std::string tiff_file_name = fs::path(_input_file).stem().string();
    std::string output_file_loc = _output_dir + "/" + tiff_file_name + ".zarr/data.zarr/0";

    
    
    json scale_metadata_list = json::array();
    for(int i=_max_level; i>=_min_level; i--){
        json scale_metadata;
        scale_metadata["path"] = std::to_string(i);
        scale_metadata_list.push_back(scale_metadata);
    }

    json combined_metadata;
    combined_metadata["datasets"] = scale_metadata_list;
    combined_metadata["version"] = "0.1";
    combined_metadata["name"] = fs::path(_input_file).stem();
    combined_metadata["metadata"] = {{"method", "mean"}};
    json final_formated_metadata;
    final_formated_metadata["multiscales"] = {combined_metadata};
    std::ofstream f(output_file_loc + "/.zattrs",std::ios_base::trunc |std::ios_base::out);
    f << final_formated_metadata;
}