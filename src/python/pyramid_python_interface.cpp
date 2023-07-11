#include "../ome_tiff_to_zarr_pyramid.h"
#include "../utilities.h"
#include <pybind11/pybind11.h>
namespace py = pybind11;

PYBIND11_MODULE(libomepyrgen, m) {
    py::class_<OmeTiffToChunkedPyramid, std::shared_ptr<OmeTiffToChunkedPyramid>>(m, "OmeTiffToChunkedPyramidCPP") \
    .def(py::init<>()) \
    .def("GenerateFromSingleFile", &OmeTiffToChunkedPyramid::GenerateFromSingleFile) \
    .def("GenerateFromCollection", &OmeTiffToChunkedPyramid::GenerateFromCollection);

    py::enum_<VisType>(m, "VisType")
        .value("NG_Zarr", VisType::NG_Zarr)
        .value("PCNG", VisType::PCNG)
        .value("Viv", VisType::Viv)
        .export_values();

    py::enum_<DSType>(m, "DSType")
        .value("Mode_Max", DSType::Mode_Max)
        .value("Mode_Min", DSType::Mode_Min)
        .value("Mean", DSType::Mean)
        .export_values();
}