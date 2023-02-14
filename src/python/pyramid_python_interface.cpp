#include "../zarr_pyramid_writer.h"
#include <pybind11/pybind11.h>
namespace py = pybind11;

PYBIND11_MODULE(libomepyrgen, m) {
    py::class_<ZarrPyramidWriter, std::shared_ptr<ZarrPyramidWriter>>(m, "ZarrPyramidWriter") \
    .def(py::init<const std::string&, const std::string& >()) \
    .def("create_base_zarr_image", &ZarrPyramidWriter::CreateBaseZarrImage, py::call_guard<py::gil_scoped_release>())\
    .def("create_pyramid_images", &ZarrPyramidWriter::CreatePyramidImages, py::call_guard<py::gil_scoped_release>())\
    .def("write_multiscale_metadata", &ZarrPyramidWriter::WriteMultiscaleMetadata);
}