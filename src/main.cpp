#include <iostream>
#include <string_view>

#include "uci/uci.h"

#ifdef DAHLIA_PROFILING_ENABLED
#include "profiling/profile_mode.h"
#endif

int main(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		if (std::string_view(argv[i]) == "--profile") {
#ifdef DAHLIA_PROFILING_ENABLED
			return run_profile_mode(argc, argv);
#else
			std::cerr << "--profile is only available on macOS builds\n";
			return 1;
#endif
		}
	}

	run_uci_loop(std::cin, std::cout);
	return 0;
}
