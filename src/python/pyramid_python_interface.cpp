#include "../ome_tiff_to_zarr_pyramid.h"
#include "../utilities.h"
#include <pybind11/pybind11.h>
namespace py = pybind11;

PYBIND11_MODULE(libomepyrgen, m) {
    py::class_<OmeTiffToChunkedPyramid, std::shared_ptr<OmeTiffToChunkedPyramid>>(m, "OmeTiffToChunkedPyramid") \
    .def(py::init<>()) \
    .def("generate_zarr_pyramid", &OmeTiffToChunkedPyramid::GenerateFromSingleFile, py::call_guard<py::gil_scoped_release>());
}