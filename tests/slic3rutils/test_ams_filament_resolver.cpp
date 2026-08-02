#include <catch2/catch_all.hpp>

#include <libslic3r/Preset.hpp>
#include <libslic3r/PresetBundle.hpp>
#include <libslic3r/PrintConfig.hpp>

#include <map>
#include <string>
#include <vector>

// Seam-level coverage for PresetBundle::set_ams_filament_resolver_fn / AmsTrayInfo (the Automation
// capability's AMS-sync filament-resolution hook, PresetBundle.hpp/.cpp). No Python/plugin layer
// involved: the resolver is a plain C++ lambda installed directly, exactly as PluginHooks.cpp would
// install one, so this proves the sync_ams_list() contract independent of the plugin system.

using namespace Slic3r;

namespace {

// The static resolver is process-global state; every test that installs one must reset it, even if
// a REQUIRE aborts the test partway through. Construct one of these first in every TEST_CASE body.
struct ResolverGuard {
    ~ResolverGuard() { PresetBundle::set_ams_filament_resolver_fn(nullptr); }
};

// Add an in-memory filament preset (no file on disk), mirroring the helper in
// tests/libslic3r/test_preset_bundle_loading.cpp.
Preset &add_inmemory_filament(PresetBundle &bundle, const std::string &name)
{
    DynamicPrintConfig config(bundle.filaments.default_preset().config);
    return bundle.filaments.load_preset(std::string(), name, config, /*select=*/false);
}

// One AMS tray's raw config, in the same shape Sidebar::build_filament_ams_list() (Plater.cpp)
// produces: every field is a single-element ConfigOptionStrings, read back via opt_string(key, 0u).
DynamicPrintConfig make_tray_config(const std::string &filament_id, const std::string &filament_type,
                                    const std::string &color, const std::string &ams_id, const std::string &slot_id)
{
    DynamicPrintConfig cfg;
    cfg.set_key_value("filament_id", new ConfigOptionStrings{filament_id});
    cfg.set_key_value("filament_type", new ConfigOptionStrings{filament_type});
    cfg.set_key_value("filament_colour", new ConfigOptionStrings{color});
    cfg.set_key_value("filament_colour_type", new ConfigOptionStrings{"0"});
    cfg.set_key_value("filament_multi_colour", new ConfigOptionStrings{});
    cfg.set_key_value("ams_id", new ConfigOptionStrings{ams_id});
    cfg.set_key_value("slot_id", new ConfigOptionStrings{slot_id});
    return cfg;
}

// Drives sync_ams_list() with a single synthetic tray, the way Sidebar::sync_ams_list() (Plater.cpp)
// does for the production call site.
unsigned int sync_single_tray(PresetBundle &bundle, const DynamicPrintConfig &tray_config, bool color_only = false)
{
    bundle.filament_ams_list.clear();
    bundle.filament_ams_list[0] = tray_config;
    std::vector<std::pair<DynamicPrintConfig *, std::string>> unknowns;
    std::map<int, AMSMapInfo> maps;
    MergeFilamentInfo merge_info;
    return bundle.sync_ams_list(unknowns, /*use_map=*/false, maps, /*enable_append=*/false, merge_info, color_only);
}

} // namespace

TEST_CASE("AMS resolver: a valid, compatible resolved name wins over stock matching", "[PresetBundle][AutomationResolver]")
{
    ResolverGuard guard;
    PresetBundle  bundle;

    // Unreachable via stock matching: no preset shares this tray's filament_id, and its name does
    // not start with "Generic <type>", so only the resolver can select it.
    add_inmemory_filament(bundle, "My Custom PLA").is_compatible = true;

    PresetBundle::set_ams_filament_resolver_fn([](const PresetBundle::AmsTrayInfo &tray) -> std::string {
        CHECK(tray.filament_id == "TEST-ID-1");
        CHECK(tray.filament_type == "PLA");
        CHECK(tray.ams_id == "0");
        CHECK(tray.slot_id == "0");
        return "My Custom PLA";
    });

    sync_single_tray(bundle, make_tray_config("TEST-ID-1", "PLA", "#112233", "0", "0"));

    REQUIRE(bundle.filament_presets.size() == 1);
    CHECK(bundle.filament_presets[0] == "My Custom PLA");
}

TEST_CASE("AMS resolver: decline or an invalid answer falls through to stock matching unchanged", "[PresetBundle][AutomationResolver]")
{
    ResolverGuard guard;
    PresetBundle  bundle;

    // Stock fallback target: matched via the existing "Generic " + filament_type heuristic once the
    // tray's raw filament_id matches nothing.
    Preset &generic    = add_inmemory_filament(bundle, "Generic PLA");
    generic.is_system     = true;
    generic.is_compatible = true;

    const DynamicPrintConfig tray = make_tray_config("UNKNOWN-ID", "PLA", "#112233", "0", "0");

    SECTION("resolver declines (empty return)")
    {
        PresetBundle::set_ams_filament_resolver_fn([](const PresetBundle::AmsTrayInfo &) { return std::string(); });
    }
    SECTION("resolver returns a name no preset has")
    {
        PresetBundle::set_ams_filament_resolver_fn([](const PresetBundle::AmsTrayInfo &) { return std::string("Does Not Exist"); });
    }
    SECTION("resolver returns a name that resolves but is not compatible")
    {
        add_inmemory_filament(bundle, "Incompatible PLA").is_compatible = false;
        PresetBundle::set_ams_filament_resolver_fn([](const PresetBundle::AmsTrayInfo &) { return std::string("Incompatible PLA"); });
    }

    sync_single_tray(bundle, tray);

    REQUIRE(bundle.filament_presets.size() == 1);
    CHECK(bundle.filament_presets[0] == "Generic PLA");
}

TEST_CASE("AMS resolver is never consulted in color-only sync mode", "[PresetBundle][AutomationResolver]")
{
    ResolverGuard guard;
    PresetBundle  bundle;

    Preset &generic    = add_inmemory_filament(bundle, "Generic PLA");
    generic.is_system     = true;
    generic.is_compatible = true;

    // color-only mode never rewrites filament_presets (colors only), so priming it with a known
    // value both gives the "unchanged" assertion a baseline and would be trivially overwritten if
    // the resolver's (deliberately different) answer were wrongly consulted.
    bundle.filament_presets = {"Generic PLA"};

    int call_count = 0;
    PresetBundle::set_ams_filament_resolver_fn([&call_count](const PresetBundle::AmsTrayInfo &) -> std::string {
        ++call_count;
        return "Should Never Be Selected";
    });

    sync_single_tray(bundle, make_tray_config("TEST-ID-2", "PLA", "#445566", "0", "0"), /*color_only=*/true);

    CHECK(call_count == 0);
    REQUIRE(bundle.filament_presets.size() == 1);
    CHECK(bundle.filament_presets[0] == "Generic PLA");
}

TEST_CASE("AMS resolver reset to nullptr restores stock behavior", "[PresetBundle][AutomationResolver]")
{
    ResolverGuard guard;
    PresetBundle  bundle;

    Preset &generic    = add_inmemory_filament(bundle, "Generic PLA");
    generic.is_system     = true;
    generic.is_compatible = true;
    add_inmemory_filament(bundle, "My Custom PLA").is_compatible = true;

    const DynamicPrintConfig tray = make_tray_config("TEST-ID-3", "PLA", "#778899", "0", "0");

    PresetBundle::set_ams_filament_resolver_fn([](const PresetBundle::AmsTrayInfo &) -> std::string { return "My Custom PLA"; });
    sync_single_tray(bundle, tray);
    REQUIRE(bundle.filament_presets.size() == 1);
    REQUIRE(bundle.filament_presets[0] == "My Custom PLA");

    // Reset (what plugin_hooks::uninstall() does before interpreter finalize) -- the very next sync
    // must fall back to stock matching, not keep using the old answer.
    PresetBundle::set_ams_filament_resolver_fn(nullptr);
    sync_single_tray(bundle, tray);
    REQUIRE(bundle.filament_presets.size() == 1);
    CHECK(bundle.filament_presets[0] == "Generic PLA");
}
