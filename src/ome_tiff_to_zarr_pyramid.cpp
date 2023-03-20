#include "ome_tiff_to_zarr_pyramid.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

void OmeTifftoZarrPyramid::GenerateFromSingleFile(  const std::string& input_file,
                                                    const std::string& output_dir, 
                                                    int min_dim, VisType v){
    TIFF *tiff_ = TIFFOpen(input_file.c_str(), "r");
    if (tiff_ != nullptr) {
        uint32_t image_width = 0, image_height = 0;
        TIFFGetField(tiff_, TIFFTAG_IMAGEWIDTH, &image_width);
        TIFFGetField(tiff_, TIFFTAG_IMAGELENGTH, &image_height);
        _max_level = static_cast<int>(ceil(log2(std::max({image_width, image_height}))));
        _min_level = static_cast<int>(ceil(log2(min_dim)));
        TIFFClose(tiff_);     
        std::string tiff_file_name = fs::path(input_file).stem().string();
        std::string zarr_file_dir = output_dir + "/" + tiff_file_name + ".zarr";
        if (v == VisType::Viv){
            zarr_file_dir = zarr_file_dir + "/data.zarr/0";
        }

        std::string base_zarr_file = zarr_file_dir + "/" + std::to_string(_max_level);
        _zpw_ptr = std::make_unique<OmeTiffToZarrConverter>();
        std::cout << "Writing base zarr image..."<<std::endl;
        _zpw_ptr->Convert(input_file, zarr_file_dir, std::to_string(_max_level), v, _th_pool);
        std::cout << "Generating image pyramids..."<<std::endl;
        _zpg_ptr = std::make_unique<ZarrBaseToPyramidGen>(zarr_file_dir, zarr_file_dir,  
                                    _max_level, _min_level);
        _zpg_ptr->CreatePyramidImages(v, _th_pool);
        WriteMultiscaleMetadataForSingleFile(input_file, output_dir, v);

    }


}

void OmeTifftoZarrPyramid::WriteMultiscaleMetadataForSingleFile(const std::string& input_file , const std::string& output_dir, VisType v)
{
    std::string tiff_file_name = fs::path(input_file).stem().string();
    std::string zarr_file_dir = output_dir + "/" + tiff_file_name + ".zarr";
    if(v == VisType::TS_Zarr){

        WriteTSZattrFile(tiff_file_name, zarr_file_dir);
    } else if (v == VisType::Viv){
        ExtractAndWriteXML(input_file, zarr_file_dir);
        WriteVivZattrFile(tiff_file_name, zarr_file_dir+"/data.zarr/0/");
        WriteVivZgroupFiles(zarr_file_dir);
    }
}


void OmeTifftoZarrPyramid::WriteMultiscaleMetadataForImageCollection(const std::string& image_file_name , const std::string& output_dir, VisType v)
{
    std::string zarr_file_dir = output_dir + "/" + image_file_name + ".zarr";
    if(v == VisType::TS_Zarr){

        WriteTSZattrFile(image_file_name, zarr_file_dir);
    } else if (v == VisType::Viv){
        _tiff_coll_to_zarr_ptr->GenerateOmeXML(image_file_name, zarr_file_dir+"/METADATA.ome.xml");                   
        WriteVivZattrFile(image_file_name, zarr_file_dir+"/data.zarr/0/");
        WriteVivZgroupFiles(zarr_file_dir);
    }
}

void OmeTifftoZarrPyramid::WriteVivZgroupFiles(const std::string& output_loc){
    std::string zgroup_text = "{\"zarr_format\": 2}";
    std::ofstream zgroup_file_1(output_loc+"/data.zarr/.zgroup", std::ios_base::out );
    if(zgroup_file_1.is_open()){
        zgroup_file_1 << zgroup_text << std::endl;
    }
    std::ofstream zgroup_file_2( output_loc + "/data.zarr/0/.zgroup", std::ios_base::out );
    if(zgroup_file_2.is_open()){
        zgroup_file_2 << zgroup_text << std::endl;
    }
}


void OmeTifftoZarrPyramid::ExtractAndWriteXML(const std::string& input_file, const std::string& xml_loc){
    TIFF *tiff_ = TIFFOpen(input_file.c_str(), "r");
    if (tiff_ != nullptr) {
        char* infobuf;
        TIFFGetField(tiff_, TIFFTAG_IMAGEDESCRIPTION , &infobuf);
        char* new_pos = strstr(infobuf, "<OME");
        std::ofstream metadata_file( xml_loc+"/METADATA.ome.xml", std::ios_base::out );
        if(metadata_file.is_open()){
            metadata_file << new_pos << std::endl;
        }
        if(!metadata_file){
            std::cout<<"Unable to write metadata file"<<std::endl;
        }
        TIFFClose(tiff_);
    }
}

void OmeTifftoZarrPyramid::WriteTSZattrFile(const std::string& tiff_file_name, const std::string& zarr_root_dir){

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
    combined_metadata["name"] = tiff_file_name;
    combined_metadata["metadata"] = {{"method", "mean"}};
    json final_formated_metadata;
    final_formated_metadata["multiscales"] = {combined_metadata};
    std::ofstream f(zarr_root_dir + "/.zattrs",std::ios_base::trunc |std::ios_base::out);
    if (f.is_open()){
        f << final_formated_metadata;
    } else {
        std::cout <<"Unable to write .zattr file at "<< zarr_root_dir << "."<<std::endl;
    }
    
}

void OmeTifftoZarrPyramid::WriteVivZattrFile(const std::string& tiff_file_name, const std::string& zattr_file_loc){
    
    json scale_metadata_list = json::array();
    for(int i=_max_level; i>=_min_level; i--){
        json scale_metadata;
        scale_metadata["path"] = std::to_string(i);
        scale_metadata_list.push_back(scale_metadata);
    }

    json combined_metadata;
    combined_metadata["datasets"] = scale_metadata_list;
    combined_metadata["version"] = "0.1";
    combined_metadata["name"] = tiff_file_name;
    combined_metadata["metadata"] = {{"method", "mean"}};
    json final_formated_metadata;
    final_formated_metadata["multiscales"] = {combined_metadata};
    std::ofstream f(zattr_file_loc + "/.zattrs",std::ios_base::trunc |std::ios_base::out);
    if (f.is_open()){   
        f << final_formated_metadata;
    } else {
        std::cout << "Unable to write .zattr file at " << zattr_file_loc << "." << std::endl;
    }

    
}


void OmeTifftoZarrPyramid::GenerateFromCollection(
                const std::string& collection_path, 
                const std::string& stitch_vector_file,
                const std::string& image_name,
                const std::string& output_dir, 
                int min_dim, 
                VisType v){
    _tiff_coll_to_zarr_ptr = std::make_unique<OmeTiffCollToZarr>(collection_path, 
                                                        stitch_vector_file);
    _max_level = static_cast<int>(ceil(log2(std::max({  _tiff_coll_to_zarr_ptr->image_width(), 
                                                        _tiff_coll_to_zarr_ptr->image_height()}))));
    _min_level = static_cast<int>(ceil(log2(min_dim)));
    

    std::string zarr_file_dir = output_dir + "/" + image_name + ".zarr";
    if (v == VisType::Viv){
        zarr_file_dir = zarr_file_dir + "/data.zarr/0";
    }

    std::string base_zarr_file = zarr_file_dir + "/" + std::to_string(_max_level);
    std::cout << "Writing base zarr image..."<<std::endl;
    _tiff_coll_to_zarr_ptr->Assemble(zarr_file_dir, std::to_string(_max_level), v, _th_pool);
    std::cout << "Generating image pyramids..."<<std::endl;
    _zpg_ptr = std::make_unique<ZarrBaseToPyramidGen>(base_zarr_file, zarr_file_dir,  
                                _max_level, _min_level);
    _zpg_ptr->CreatePyramidImages(v, _th_pool);
    WriteMultiscaleMetadataForImageCollection(image_name, output_dir, v);

}