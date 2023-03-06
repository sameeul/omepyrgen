#pragma once

#include <string>
#include <tiffio.h>
#include <cmath>
#include <memory>
#include "ome_tiff_to_zarr_converter.h"
#include "zarr_pyramid_assembler.h"
#include "zarr_base_to_pyr_gen.h"
#include "utils.h"
#include "BS_thread_pool.hpp"

class OmeTifftoZarrPyramid{
public:
    OmeTifftoZarrPyramid(){}
    void GenerateFromSingleFile(const std::string& input_file, const std::string& output_dir, 
                                int min_dim, VisType v);
    
private:
    std::string _input_file, _output_dir;
    std::unique_ptr<OmeTiffToZarrConverter> _zpw_ptr = nullptr;
    std::unique_ptr<ZarrBaseToPyramidGen> _zpg_ptr = nullptr;
    int _max_level, _min_level;
    BS::thread_pool _th_pool;

    void WriteMultiscaleMetadata(   const std::string& input_file, 
                                    const std::string& output_dir,
                                    VisType v);
    void ExtractAndWriteXML(const std::string& input_file, const std::string& xml_loc);
    void WriteTSZattrFile(const std::string& tiff_file_name, const std::string& zattr_file_loc);
    void WriteVivZattrFile(const std::string& tiff_file_name, const std::string& zattr_file_loc);
    void WriteVivZgroupFiles(const std::string& output_dir);
     
};