#include <iostream>
#include <string>
#include "../src/zarr_pyramid_writer.h"
#include "../src/zarr_pyramid_assembler.h"
#include<chrono>

void test_zarr_pyramid_writer(){
    std::string input_file = "/home/samee/axle/data/r001_c001_z000.ome.tif";
    std::string output_file = "/home/samee/axle/data/r001_c001_z000_ome_zarr";
    
    input_file = "/home/samee/axle/data/test_image.ome.tif";
    output_file = "/home/samee/axle/data/test_image_ome_zarr_2";

    auto zpw = ZarrPyramidWriter(input_file, output_file);
    auto t1 = std::chrono::high_resolution_clock::now();
    zpw.CreateBaseZarrImage();
    //zpw.CreateBaseZarrImageV2();
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et1 = t2-t1;
    std::cout << "time for base image: "<< et1.count() << std::endl;
//    zpw.CreatePyramidImages();
//    zpw.WriteMultiscaleMetadata();
    auto t3 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et2 = t3-t1;
    std::cout << "time for full image: "<< et2.count() << std::endl;
}

void test_zarr_pyramid_assembler()
{
    std::string input_dir = "/home/samee/axle/data/intensity1";
    std::string output_file = "/home/samee/axle/data/test_assembly";
    std::string stitch_vector = "/home/samee/axle/data/tmp_dir/img-global-positions-1.txt";

    auto zpw = ZarrPyramidAssembler(input_dir, output_file, stitch_vector);
    auto t1 = std::chrono::high_resolution_clock::now();
    zpw.CreateBaseZarrImage();
    //zpw.CreateBaseZarrImageV2();

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et1 = t2-t1;
    std::cout << "time for base image: "<< et1.count() << std::endl;

}



int main(){
    std::cout<<"hello"<<std::endl;
    //test_zarr_pyramid_writer();
    test_zarr_pyramid_assembler();
}
