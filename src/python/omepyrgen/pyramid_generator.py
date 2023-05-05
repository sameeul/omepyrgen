from libomepyrgen import OmeTiffToChunkedPyramidCPP
from libomepyrgen import VisType

class PyramidGenerartor:
    def __init__(self) -> None:
        self._pyr_generator = OmeTiffToChunkedPyramidCPP()
    
    def generate_from_single_image(self, input_file, output_dir, min_dim, vis_type):
        if vis_type == "TS_Zarr":
            self._pyr_generator.GenerateFromSingleFile(input_file, output_dir, min_dim, VisType.TS_Zarr)
        if vis_type == "TS_NPC":
            self._pyr_generator.GenerateFromSingleFile(input_file, output_dir, min_dim, VisType.TS_NPC)
        if vis_type == "Viv":
            self._pyr_generator.GenerateFromSingleFile(input_file, output_dir, min_dim, VisType.Viv)

    def generate_from_image_collection(self, collection_path, stitch_vector_file, image_name, output_dir, min_dim, vis_type):
        if vis_type == "TS_Zarr":
            self._pyr_generator.GenerateFromCollection(collection_path, stitch_vector_file, image_name, output_dir, min_dim, VisType.TS_Zarr)
        if vis_type == "TS_NPC":
            self._pyr_generator.GenerateFromCollection(collection_path, stitch_vector_file, image_name, output_dir, min_dim, VisType.TS_NPC)
        if vis_type == "Viv":
            self._pyr_generator.GenerateFromCollection(collection_path, stitch_vector_file, image_name, output_dir, min_dim, VisType.Viv)
