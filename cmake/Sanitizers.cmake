# Opt-in ASan+UBSan build variant (see REFERENCE.md 2.2).
# TSan is deferred until the engine gains real multithreading (Lazy-SMP).

function(dahlia_enable_sanitizers target)
	if (NOT DAHLIA_ENABLE_SANITIZERS)
		return()
	endif()

	target_compile_options(${target} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
	target_link_options(${target} PUBLIC -fsanitize=address,undefined)
endfunction()
