include(FetchContent)

# Vulkan
find_package(Vulkan REQUIRED COMPONENTS volk shaderc_combined)

# SDL3
find_package(SDL3 3.4.12 QUIET)
if (NOT SDL3_FOUND)
	FetchContent_Declare(
		SDL3
		GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
		GIT_TAG f87239e71e42da91ca317a12eefb82cfbf3393eb # release-3.4.12
	)
	FetchContent_GetProperties(SDL3)
	if (NOT SDL3_POPULATED)
		FetchContent_MakeAvailable(SDL3)
	endif()
endif()

# GLM
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