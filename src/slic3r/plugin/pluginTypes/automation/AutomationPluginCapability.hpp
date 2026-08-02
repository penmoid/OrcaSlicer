#pragma once
#include "slic3r/plugin/PythonPluginInterface.hpp"
#include <pybind11/pybind11.h>
#include <string>

namespace Slic3r {

// Context handed to Automation plugins for the AMS-sync filament-resolution event. Populated by
// the installed hook (PluginHooks.cpp) from PresetBundle::AmsTrayInfo plus the current printer
// preset name. Read-only from Python.
struct AmsFilamentResolveContext {
    std::string filament_id;     // tray RFID/system id, e.g. "GFG96"
    std::string filament_type;   // e.g. "PETG"
    std::string filament_color;  // hex string as present on the tray config
    std::string ams_id;
    std::string slot_id;
    std::string printer_preset;  // current printer preset name
};

class AutomationPluginCapability : public PluginCapabilityInterface {
public:
    PluginCapabilityType get_type() const override { return PluginCapabilityType::Automation; }

    // Runs synchronously on the UI thread (AMS sync is a wxBusyCursor click handler): keep
    // resolvers fast and pure, no orca.host.ui.* marshaling -- a slow resolver blocks the sync
    // click. Not pure virtual: this is the first of what may become several Automation events, so
    // it defaults to Skipped and plugins only override the events they actually handle.
    virtual ExecutionResult resolve_ams_filament(AmsFilamentResolveContext &) { return ExecutionResult::skipped(); }

    static void RegisterBindings(pybind11::module_ &module, pybind11::enum_<PluginCapabilityType> &pluginTypes);
};

} // namespace Slic3r
