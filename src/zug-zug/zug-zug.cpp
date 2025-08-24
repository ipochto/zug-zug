#include <cxxopts.hpp>
#include <fmt/base.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include "utils/filesystem.hpp"

void parseCmdLineArguments(int argc, char* argv[])
{
	auto options = cxxopts::Options{"Zug-Zug", 
									"Just an engine for classical 2D RTS games. Dabu..."};
	options.add_options()
		("h,help", "Print usage")
		("d,data", "Path to game data", cxxopts::value<fs::path>());;
	
	options.allow_unrecognised_options();
	const auto parsed = options.parse(argc, argv);
	
	const auto unmatched = parsed.unmatched();
	if (!unmatched.empty()) {
		fmt::println("Unrecognized command line argument(s): {}\n", unmatched);
		fmt::println("{}", options.help());
		exit(1);
	}	
	if (parsed.count("help")) {
		fmt::println("{}", options.help());
		exit(0);
	}
	if (parsed.count("data")) {
		const auto dataPath = parsed["data"].as<fs::path>();
		spdlog::info("Using given data path: \"{}\"", dataPath.string());
	}
}

int zzMain(int argc, char* argv[])
{
	parseCmdLineArguments(argc, argv);
	return 0;
}