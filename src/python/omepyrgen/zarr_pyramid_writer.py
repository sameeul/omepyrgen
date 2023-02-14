from libomepyrgen import ZarrPyramidWriter as ZarrPyramidWriterCPP

class ZarrPyramidWriter:
    def __init__(self, input_file, output_file) -> None:
        self._writer = ZarrPyramidWriterCPP(input_file, output_file)
    
    def create_base_zarr_image(self):
        self._writer.create_base_zarr_image()

    def create_pyramid_images(self):
        self._writer.create_pyramid_images()

    def write_multiscale_metadata(self):
        self._writer.write_multiscale_metadata()

    
