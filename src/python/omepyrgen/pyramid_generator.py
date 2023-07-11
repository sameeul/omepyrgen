from .libomepyrgen import OmeTiffToChunkedPyramidCPP, VisType, DSType

class PyramidGenerartor:
    def __init__(self) -> None:
        self._pyr_generator = OmeTiffToChunkedPyramidCPP()
        self.vis_types_dict ={ "NG_Zarr" : VisType.NG_Zarr, "PCNG" : VisType.PCNG, "Viv" : VisType.Viv}
        self.ds_types_dict = {"mean" : DSType.Mean, "mode_max" : DSType.Mode_Max, "mode_min" : DSType.Mode_Min}

    def generate_from_single_image(self, input_file, output_dir, min_dim, vis_type, ds_type):
        self._pyr_generator.GenerateFromSingleFile(input_file, output_dir, min_dim, self.vis_types_dict[vis_type], self.ds_types_dict[ds_type])

    def generate_from_image_collection(self, collection_path, pattern , image_name, output_dir, min_dim, vis_type, ds_type):   
        self._pyr_generator.GenerateFromCollection(collection_path, pattern , image_name, output_dir, min_dim, self.vis_types_dict[vis_type], self.ds_types_dict[ds_type])
