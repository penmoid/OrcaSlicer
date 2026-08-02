#pragma once
#include "AutomationPluginCapability.hpp"
#include "slic3r/plugin/PyPluginTrampoline.hpp"
#include "slic3r/plugin/PluginAuditManager.hpp"

namespace Slic3r {
class PyAutomationPluginCapabilityTrampoline : public PyPluginCommonTrampoline<AutomationPluginCapability> {
public:
    using PyPluginCommonTrampoline<AutomationPluginCapability>::PyPluginCommonTrampoline;
    ExecutionResult resolve_ams_filament(AmsFilamentResolveContext &ctx) override {
        ORCA_PY_OVERRIDE_AUDITED(
            ::Slic3r::PluginAuditManager::AuditMode::Loading,
            [] {},
            PYBIND11_OVERRIDE,
            ExecutionResult, AutomationPluginCapability, resolve_ams_filament, ctx);
    }
};
} // namespace Slic3r
