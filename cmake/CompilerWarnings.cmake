# Shared warning flags for Dahlia targets.

function(dahlia_set_warnings target)
	set(MSVC_WARNINGS
		/W4
	)

	set(CLANG_GCC_WARNINGS
		-Wall
		-Wextra
		-Wpedantic
		-Wshadow
		-Wconversion
		-Wsign-conversion
		-Wnon-virtual-dtor
		-Wold-style-cast
		-Wcast-align
		-Wunused
		-Woverloaded-virtual
		-Wnull-dereference
		-Wdouble-promotion
	)

	if (MSVC)
		set(PROJECT_WARNINGS ${MSVC_WARNINGS})
	else()
		set(PROJECT_WARNINGS ${CLANG_GCC_WARNINGS})
	endif()

	if (DAHLIA_WARNINGS_AS_ERRORS)
		list(APPEND PROJECT_WARNINGS -Werror)
	endif()

	target_compile_options(${target} PRIVATE ${PROJECT_WARNINGS})
endfunction()
