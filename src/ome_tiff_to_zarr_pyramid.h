#pragma once

#include <string>
#include <tiffio.h>
#include <cmath>
#include <memory>
#include "ome_tiff_to_zarr_converter.h"
#include "zarr_pyramid_assembler.h"
#include "zarr_base_to_pyr_gen.h"
#include "utilities.h"
#include "BS_thread_pool.hpp"

class OmeTiffToChunkedPyramid{
public:
    OmeTiffToChunkedPyramid(){}
    void GenerateFromSingleFile(const std::string& input_file, const std::string& output_dir, 
                                int min_dim, VisType v, DSType ds);
    void GenerateFromCollection(const std::string& collection_path, const std::string& stitch_vector_file,
                                const std::string& image_name, const std::string& output_dir, 
                                int min_dim, VisType v, DSType ds);

private:
    std::unique_ptr<OmeTiffToZarrConverter> _zpw_ptr = nullptr;
    std::unique_ptr<ChunkedBaseToPyramid> _zpg_ptr = nullptr;
    std::unique_ptr<OmeTiffCollToChunked> _tiff_coll_to_zarr_ptr = nullptr;
    BS::thread_pool _th_pool;

    void WriteMultiscaleMetadataForImageCollection( const std::string& input_file, 
                                                    const std::string& output_dir,
                                                    int min_level, int max_level,
                                                    VisType v);
    void WriteMultiscaleMetadataForSingleFile(  const std::string& input_file, 
                                                const std::string& output_dir,
                                                int min_level, int max_level,
                                                VisType v);
    void ExtractAndWriteXML(const std::string& input_file, const std::string& xml_loc);
    void WriteTSZattrFile(const std::string& tiff_file_name, const std::string& zattr_file_loc, int min_level, int max_level);
    void WriteVivZattrFile(const std::string& tiff_file_name, const std::string& zattr_file_loc, int min_level, int max_level);
    void WriteVivZgroupFiles(const std::string& output_dir);
     
};