#include "../ome_tiff_to_zarr_pyramid.h"
#include "../utilities.h"
#include <pybind11/pybind11.h>
namespace py = pybind11;

PYBIND11_MODULE(libomepyrgen, m) {
    py::class_<OmeTifftoZarrPyramid, std::shared_ptr<OmeTifftoZarrPyramid>>(m, "OmeTifftoZarrPyramid") \
    .def(py::init<>()) \
    .def("generate_zarr_pyramid", &OmeTifftoZarrPyramid::GenerateFromSingleFile, py::call_guard<py::gil_scoped_release>());
}