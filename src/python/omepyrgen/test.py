from zarr_pyramid_writer import ZarrPyramidWriter
from time import time
if __name__=="__main__":
    input_file = "/mnt/hdd8/axle/data/bfio_test_images/r001_c001_z000.ome.tif"
    output_file = "/mnt/hdd8/axle/data/bfio_test_images/r001_c001_z000_ome_zarr"

    start_time = time()
    zpw = ZarrPyramidWriter(input_file, output_file )
    zpw.create_base_zarr_image()
    zpw.create_pyramid_images()
    zpw.write_multiscale_metadata()

    end_time = time()
    print(f"elapsed time = {end_time - start_time}")
