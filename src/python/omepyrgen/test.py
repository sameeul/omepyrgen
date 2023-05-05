from pyramid_generator import PyramidGenerartor
from time import time

def test_single_image():
    input_file = "/home/samee/axle/data/test_image.ome.tif"
    output_dir = "/home/samee/axle/data/test_image_ome_zarr_2"

    start_time = time()
    pyr_gen = PyramidGenerartor()

    pyr_gen.generate_from_single_image(input_file, output_dir, 1024, "TS_Zarr")
    end_time = time()
    print(f"elapsed time = {end_time - start_time}")

def test_image_collection():
    input_dir = "/home/samee/axle/data/intensity1"
    stitch_vector_file = "/home/samee/axle/data/tmp_dir/img-global-positions-1.txt";
    output_dir = "/home/samee/axle/data/test_assembly_out"
    image_name = "test_image"
    start_time = time()
    pyr_gen = PyramidGenerartor()
    pyr_gen.generate_from_image_collection(input_dir, stitch_vector_file, image_name, output_dir, 1024, "Viv")
    end_time = time()
    print(f"elapsed time = {end_time - start_time}")

if __name__=="__main__":
    #test_single_image()
    test_image_collection()