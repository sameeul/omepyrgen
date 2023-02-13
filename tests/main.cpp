#include <iostream>
#include <string>
#include "../src/zarr_pyramid_writer.h"
#include<chrono>

void test_zarr_pyramid_writer(){
    std::string input_file = "/mnt/hdd8/axle/data/bfio_test_images/r001_c001_z000.ome.tif";
    std::string output_file = "/mnt/hdd8/axle/data/bfio_test_images/r001_c001_z000_ome_zarr";

    auto zpw = ZarrPyramidWriter(input_file, output_file);
    auto t1 = std::chrono::high_resolution_clock::now();
    zpw.CreateBaseZarrImage();
    //zpw.CreateBaseZarrImageV2();
    zpw.CreatePyramidImages();
    zpw.WriteMultiscaleMetadata();
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> et1 = t2-t1;
    std::cout << "time for base image: "<< et1.count() << std::endl;
}

int main(){
    std::cout<<"hello"<<std::endl;
    test_zarr_pyramid_writer();
}
