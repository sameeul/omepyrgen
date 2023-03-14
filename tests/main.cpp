#include <iostream>
#include <string>
#include "../src/ome_tiff_to_zarr_converter.h"
#include "../src/zarr_pyramid_assembler.h"
#include "../src/zarr_base_to_pyr_gen.h"
#include "../src/ome_tiff_to_zarr_pyramid.h"
#include "../src/utilities.h"
#include "BS_thread_pool.hpp"
#include<chrono>

void test_zarr_pyramid_writer(){
    std::string input_file = "/home/samee/axle/data/r001_c001_z000.ome.tif";
    std::string output_file = "/home/samee/axle/data/r001_c001_z000_ome_zarr";
    
    //input_file = "/home/samee/axle/data/test_image.ome.tif";
    //output_file = "/home/samee/axle/data/test_image_ome_zarr_2";

    auto zpw = OmeTiffToZarrConverter();
    auto t1 = std::chrono::high_resolution_clock::now();
    BS::thread_pool th_pool;
    zpw.Convert(input_file, output_file, 16, VisType::TS_Zarr ,th_pool);
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et1 = t2-t1;
    std::cout << "time for base image: "<< et1.count() << std::endl;
}

void test_zarr_pyramid_assembler()
{
    std::string input_dir = "/home/samee/axle/data/intensity1";
    std::string output_file = "/home/samee/axle/data/test_assembly";
    std::string stitch_vector = "/home/samee/axle/data/tmp_dir/img-global-positions-1.txt";

    auto zpw = OmeTiffCollToZarr(input_dir, stitch_vector);
    auto t1 = std::chrono::high_resolution_clock::now();
    BS::thread_pool th_pool;
    zpw.Assemble(output_file, VisType::Viv, th_pool);

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et1 = t2-t1;
    std::cout << "time for base image: "<< et1.count() << std::endl;

}

void test_zarr_pyramid_gen(){
    std::string input_zarr_dir = "/home/samee/axle/data/test_assembly";
    std::string output_root_dir = "/home/samee/axle/data/test_assembly_out";
    auto t1 = std::chrono::high_resolution_clock::now();
    auto zarr_pyr_gen = ZarrBaseToPyramidGen(input_zarr_dir, output_root_dir, 17, 10 );
    BS::thread_pool th_pool;
    zarr_pyr_gen.CreatePyramidImages(VisType::Viv, th_pool);
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et1 = t2-t1;
    std::cout << "time for base image: "<< et1.count() << std::endl;
}

void test_ome_tiff_to_zarr_pyramid_gen(){
    std::string input_tiff_file = "/home/samee/axle/data/r001_c001_z000.ome.tif";
    input_tiff_file = "/home/samee/axle/data/test_image.ome.tif";
    std::string output_dir = "/home/samee/axle/data/test_assembly_out";
    auto t1 = std::chrono::high_resolution_clock::now();
    auto zarr_pyr_gen = OmeTifftoZarrPyramid();
    zarr_pyr_gen.GenerateFromSingleFile(input_tiff_file, output_dir, 1024, VisType::Viv);
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et1 = t2-t1;
    std::cout << "time for base image: "<< et1.count() << std::endl;
}

void test_ome_tiff_coll_to_zarr_pyramid_gen(){
    std::string input_dir = "/home/samee/axle/data/intensity1";
    std::string stitch_vector = "/home/samee/axle/data/tmp_dir/img-global-positions-1.txt";
    std::string image_name = "test_image";
    std::string output_dir = "/home/samee/axle/data/test_assembly_out";
    auto t1 = std::chrono::high_resolution_clock::now();
    auto zarr_pyr_gen = OmeTifftoZarrPyramid();
    zarr_pyr_gen.GenerateFromCollection(input_dir, stitch_vector, image_name, output_dir, 1024, VisType::Viv);
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et1 = t2-t1;
    std::cout << "time for base image: "<< et1.count() << std::endl;
}

void test_ome_tiff_coll_to_zarr_pyramid_gen_xml(){
    std::string input_dir = "/home/samee/axle/data/intensity1";
    std::string output_file = "/home/samee/axle/data/test_assembly/test.xml";
    std::string stitch_vector = "/home/samee/axle/data/tmp_dir/img-global-positions-1.txt";
    std::string image_name = "test_image";
    auto zpw = OmeTiffCollToZarr(input_dir, stitch_vector);
    auto t1 = std::chrono::high_resolution_clock::now();
    BS::thread_pool th_pool;
    //zpw.Assemble(output_file, VisType::Viv, th_pool);
    zpw.GenerateOmeXML(image_name, output_file);

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et1 = t2-t1;
    std::cout << "time for base image: "<< et1.count() << std::endl;
}


int main(){
    std::cout<<"hello"<<std::endl;
    test_zarr_pyramid_writer();
    //test_zarr_pyramid_assembler();
    //test_zarr_pyramid_gen();
    //test_ome_tiff_to_zarr_pyramid_gen();
    //test_ome_tiff_coll_to_zarr_pyramid_gen();
    //test_ome_tiff_coll_to_zarr_pyramid_gen_xml();
}
