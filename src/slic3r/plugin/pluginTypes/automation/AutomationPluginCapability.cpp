#include "AutomationPluginCapability.hpp"
#include "AutomationPluginCapabilityTrampoline.hpp"

#include <boost/log/trivial.hpp>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
namespace Slic3r {

void AutomationPluginCapability::RegisterBindings(py::module_ &module, py::enum_<PluginCapabilityType> &pluginTypes)
{
    (void) pluginTypes; // unused: Automation has no capability-specific enum to extend (a single event, no Step-like variants).
    BOOST_LOG_TRIVIAL(debug) << "Registering orca.automation bindings";

    auto automation = module.def_submodule("automation", "Automation Plugins API (application-event hooks).");

    py::class_<AmsFilamentResolveContext>(automation, "AmsFilamentResolveContext")
        .def_readonly("filament_id", &AmsFilamentResolveContext::filament_id)
        .def_readonly("filament_type", &AmsFilamentResolveContext::filament_type)
        .def_readonly("filament_color", &AmsFilamentResolveContext::filament_color)
        .def_readonly("ams_id", &AmsFilamentResolveContext::ams_id)
        .def_readonly("slot_id", &AmsFilamentResolveContext::slot_id)
        .def_readonly("printer_preset", &AmsFilamentResolveContext::printer_preset);

    py::class_<AutomationPluginCapability, PluginCapabilityInterface,
               PyAutomationPluginCapabilityTrampoline,
               std::shared_ptr<AutomationPluginCapability>>(automation, "AutomationCapabilityBase")
        .def(py::init<>())
        .def("get_type", &AutomationPluginCapability::get_type)
        .def("resolve_ams_filament", &AutomationPluginCapability::resolve_ams_filament);
}

} // namespace Slic3r
