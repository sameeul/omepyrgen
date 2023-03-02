from zarr_pyramid_writer import ZarrPyramidWriter
from time import time
if __name__=="__main__":
    input_file = "/home/samee/axle/data/test_image.ome.tif"
    output_file = "/home/samee/axle/data/test_image_ome_zarr_2"

    start_time = time()
    zpw = ZarrPyramidWriter(input_file, output_file )
    zpw.create_base_zarr_image()
    zpw.create_pyramid_images()
    zpw.write_multiscale_metadata()

    end_time = time()
    print(f"elapsed time = {end_time - start_time}")
