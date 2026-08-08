include(FetchContent)

# --- Vulkan core (loader + headers) — always required ---
find_package(Vulkan REQUIRED)

# ============================== volk ==============================
find_package(Vulkan QUIET COMPONENTS volk)

if (NOT TARGET Vulkan::volk)
	find_package(volk CONFIG QUIET)
	if (TARGET volk::volk)
		add_library(Vulkan::volk ALIAS volk::volk)
	endif()
endif()

if (NOT TARGET Vulkan::volk)
	message(STATUS "volk not found via SDK or system package — building from source")
	FetchContent_Declare(
		volk
		GIT_REPOSITORY https://github.com/zeux/volk.git
		GIT_TAG 3ca312a4f38baa63d8006b6905abbeeb89c8087d
	)
	FetchContent_MakeAvailable(volk)
	add_library(Vulkan::volk ALIAS volk)
endif()

# ========================== shaderc_combined =======================
find_package(Vulkan QUIET COMPONENTS shaderc_combined)

if (NOT TARGET Vulkan::shaderc_combined)
	find_package(PkgConfig QUIET)
	if (PkgConfig_FOUND)
		pkg_check_modules(SHADERC_COMBINED QUIET IMPORTED_TARGET shaderc_combined)
		if (TARGET PkgConfig::SHADERC_COMBINED)
			add_library(Vulkan::shaderc_combined ALIAS PkgConfig::SHADERC_COMBINED)
		endif()
	endif()
endif()

if (NOT TARGET Vulkan::shaderc_combined)
	message(FATAL_ERROR
		"shaderc_combined not found.\n"
		"  Windows: install the Vulkan SDK (https://vulkan.lunarg.com/sdk/home).\n"
		"  Arch:    sudo pacman -S shaderc\n"
		"  Ubuntu/Debian: sudo apt install libshaderc-dev\n"
		"  Fedora:  sudo dnf install shaderc-devel")
endif()

# ============================== SDL3 ===============================
if (NOT SDL3_FOUND)
	set(SDL_SHARED OFF CACHE BOOL "" FORCE)
	set(SDL_STATIC ON CACHE BOOL "" FORCE)

	FetchContent_Declare(
		SDL3
		GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
		GIT_TAG f87239e71e42da91ca317a12eefb82cfbf3393eb
	)
	FetchContent_GetProperties(SDL3)
	if (NOT SDL3_POPULATED)
		FetchContent_MakeAvailable(SDL3)
	endif()
endif()

# ============================== GLM ================================
find_package(glm 1.0.3 QUIET)
if (NOT glm_FOUND)
	FetchContent_Declare(
		glm
		GIT_REPOSITORY https://github.com/g-truc/glm.git
		GIT_TAG 8d1fd52e5ab5590e2c81768ace50c72bae28f2ed # release-1.0.3
	)
	FetchContent_GetProperties(glm)
	if (NOT glm_POPULATED)
		set(FETCHCONTENT_QUIET NO)
		FetchContent_MakeAvailable(glm)
		set_target_properties(glm PROPERTIES FOLDER "Dependencies")
	endif()
endif()