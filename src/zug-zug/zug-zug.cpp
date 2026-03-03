#include <cxxopts.hpp>
#include <filesystem>
#include <print>
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
		std::println("Unrecognized command line argument(s): {}", unmatched);
		std::println("{}", options.help());
		exit(1);
	}	
	if (parsed.count("help")) {
		std::println("{}", options.help());
		exit(0);
	}
	if (parsed.count("data")) {
		const auto dataPath = fs::absolute(parsed["data"].as<fs::path>()).lexically_normal();
		if (fs::exists(dataPath)) {
			spdlog::info("Using given data path: \"{}\"", dataPath.string());
			/// TODO: -> set in the game config
		} else {
			std::println("The specified path to game data does not exist: \"{}\"", dataPath.string());
			exit(1);
		}
	}
}

int zzMain(int argc, char* argv[])
{
	parseCmdLineArguments(argc, argv);
	return 0;
}
