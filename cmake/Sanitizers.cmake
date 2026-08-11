# Opt-in sanitizer build variants (see REFERENCE.md 2.2). ASan+UBSan and TSan
# are mutually exclusive instrumentation (both intercept the allocator/runtime
# in incompatible ways), so they're separate options -- never enable both for
# the same target.

function(dahlia_enable_sanitizers target)
	if (NOT DAHLIA_ENABLE_SANITIZERS)
		return()
	endif()

	target_compile_options(${target} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
	target_link_options(${target} PUBLIC -fsanitize=address,undefined)
endfunction()

# ThreadSanitizer: added alongside async search (REFERENCE.md 3.8's SMP note,
# 3.10's stop/isready handling) since that's the point the engine gained a
# second thread and data races became possible for the first time.
function(dahlia_enable_tsan target)
	if (NOT DAHLIA_ENABLE_TSAN)
		return()
	endif()

	target_compile_options(${target} PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
	target_link_options(${target} PUBLIC -fsanitize=thread)
endfunction()
