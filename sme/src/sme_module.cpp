// Python.h (included by pybind11.h) must come first
// https://docs.python.org/3.2/c-api/intro.html#include-files
#include <pybind11/pybind11.h>

#include "sme_module.hpp"
#include "version.hpp"

namespace sme {

void pybindModule(pybind11::module &m) {
  m.doc() = R"(
            Spatial Model Editor Python interface

            Python bindings to a subset of the functionality
            available in the full GUI Spatial Model Editor

            https://spatial-model-editor.readthedocs.io/
            )";
  m.def("open_sbml_file", openSbmlFile, pybind11::arg("filename"),
        R"(
        opens an SBML file containing a spatial model

        Args:
            filename (str): the SBML file to open

        Returns:
            Model: the spatial model
        )");
  m.def("open_example_model", openExampleModel,
        R"(
        opens a built in example spatial model

        Returns:
            Model: the example spatial model
        )");
  m.attr("__version__") = SPATIAL_MODEL_EDITOR_VERSION;
}

Model openSbmlFile(const std::string &filename) { return Model(filename); }

Model openExampleModel() { return Model(":/models/very-simple-model.xml"); }

} // namespace sme

//

//

//

//

//

//

//

//

//

//

//

//

//
