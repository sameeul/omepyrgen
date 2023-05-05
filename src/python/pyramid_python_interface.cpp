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
        .value("TS_Zarr", VisType::TS_Zarr)
        .value("TS_NPC", VisType::TS_NPC)
        .value("Viv", VisType::Viv)
        .export_values();
}