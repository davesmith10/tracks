#include "config.h"

#include <iostream>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace tracks {

static void load_yaml(Config& cfg, const std::string& path) {
    std::ifstream fin(path);
    if (!fin.is_open()) return;

    YAML::Node root = YAML::Load(fin);

    if (auto net = root["network"]) {
        if (net["multicast_group"]) cfg.multicast_group = net["multicast_group"].as<std::string>();
        if (net["port"])            cfg.port = net["port"].as<uint16_t>();
        if (net["ttl"])             cfg.ttl  = net["ttl"].as<int>();
        if (net["loopback"])        cfg.loopback  = net["loopback"].as<bool>();
        if (net["interface"])       cfg.interface = net["interface"].as<std::string>();
    }
    if (auto an = root["analysis"]) {
        if (an["sample_rate"]) cfg.sample_rate = an["sample_rate"].as<int>();
        if (an["frame_size"])  cfg.frame_size  = an["frame_size"].as<int>();
        if (an["hop_size"])    cfg.hop_size    = an["hop_size"].as<int>();
    }
    if (auto tr = root["transport"]) {
        if (tr["position_interval"]) cfg.position_interval = tr["position_interval"].as<double>();
    }
    if (auto ev = root["events"]) {
        if (ev["continuous_interval"]) cfg.continuous_interval = ev["continuous_interval"].as<double>();
    }
}

static void print_available_events() {
    std::cout << "\nAvailable event types:\n";
    const auto& names = event_name_map();
    // Print sorted by name
    std::vector<std::string> sorted_names;
    sorted_names.reserve(names.size());
    for (const auto& [name, type] : names) {
        if (!is_transport_event(type)) {
            sorted_names.push_back(name);
        }
    }
    std::sort(sorted_names.begin(), sorted_names.end());
    for (const auto& name : sorted_names) {
        std::cout << "  " << name << "\n";
    }
}

bool load_config(Config& cfg, int argc, char* argv[]) {
    po::options_description desc("TRACKS - audio event emitter");
    desc.add_options()
        ("help,h",    "Show help")
        ("input,i",   po::value<std::string>(), "Input audio file (required)")
        ("config,c",  po::value<std::string>(), "Config YAML file")
        ("multicast-group", po::value<std::string>(), "Multicast group address")
        ("port,p",    po::value<uint16_t>(),    "UDP port")
        ("ttl",       po::value<int>(),         "Multicast TTL")
        ("loopback",  po::value<bool>(),        "Enable multicast loopback")
        ("interface", po::value<std::string>(), "Outbound interface address")
        ("sample-rate",        po::value<int>(),    "Analysis sample rate")
        ("frame-size",         po::value<int>(),    "Analysis frame size")
        ("hop-size",           po::value<int>(),    "Analysis hop size")
        ("position-interval",  po::value<double>(), "Seconds between position heartbeats")
        ("events,e",  po::value<std::string>(), "Comma-separated event types (e.g. beat,onset,pitch)")
        ("all",       "Enable all event types")
        ("primary",   "Enable tier 1 events (beat, onset, silence, loudness, energy)")
        ("continuous-interval", po::value<double>(), "Seconds between continuous event emissions (default 0.1)")
        ("list-events", "List all available event types and exit")
    ;

    po::positional_options_description pos;
    pos.add("input", 1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv)
            .options(desc).positional(pos).run(), vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n" << desc << "\n";
        return false;
    }

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return false;
    }

    if (vm.count("list-events")) {
        print_available_events();
        return false;
    }

    // Load YAML config (default or specified)
    if (vm.count("config")) {
        load_yaml(cfg, vm["config"].as<std::string>());
    } else {
        // Try default locations
        load_yaml(cfg, "config/tracks-default.yaml");
        load_yaml(cfg, "tracks-default.yaml");
    }

    // CLI overrides
    if (vm.count("input"))             cfg.input_file       = vm["input"].as<std::string>();
    if (vm.count("multicast-group"))   cfg.multicast_group  = vm["multicast-group"].as<std::string>();
    if (vm.count("port"))              cfg.port             = vm["port"].as<uint16_t>();
    if (vm.count("ttl"))               cfg.ttl              = vm["ttl"].as<int>();
    if (vm.count("loopback"))          cfg.loopback         = vm["loopback"].as<bool>();
    if (vm.count("interface"))         cfg.interface        = vm["interface"].as<std::string>();
    if (vm.count("sample-rate"))       cfg.sample_rate      = vm["sample-rate"].as<int>();
    if (vm.count("frame-size"))        cfg.frame_size       = vm["frame-size"].as<int>();
    if (vm.count("hop-size"))          cfg.hop_size         = vm["hop-size"].as<int>();
    if (vm.count("position-interval")) cfg.position_interval= vm["position-interval"].as<double>();
    if (vm.count("continuous-interval")) cfg.continuous_interval = vm["continuous-interval"].as<double>();

    // Event filter: --all > --primary > --events > default
    if (vm.count("all")) {
        cfg.enabled_events = all_events();
    } else if (vm.count("primary")) {
        cfg.enabled_events = tier1_events();
    } else if (vm.count("events")) {
        cfg.enabled_events = parse_event_filter(vm["events"].as<std::string>());
        if (cfg.enabled_events.empty()) {
            std::cerr << "Error: no valid events specified\n";
            return false;
        }
    } else {
        cfg.enabled_events = default_events();
    }

    if (cfg.input_file.empty()) {
        std::cerr << "Error: no input file specified\n" << desc << "\n";
        return false;
    }

    return true;
}

} // namespace tracks
