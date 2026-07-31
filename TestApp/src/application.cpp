#include "application.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <vector>

bool Application::initialize()
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO))
	{
		window = SDL_CreateWindow("Vulkan 3D Renderer",
			width,
			height,
			SDL_WINDOW_VULKAN |
			SDL_WINDOW_RESIZABLE);

		if (!window)
		{
			std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
			return false;
		}

		if (!initializeVulkan())
		{
			return false;
		}
	}
	else
	{
		std::cerr << "Error initializing SDL: " << SDL_GetError() << std::endl;
		return false;
	}

	return true;
}

void Application::run()
{
	// Main application loop code here
}

void Application::shutdown()
{
	if (vulkanInstance)
	{
		vkDestroyInstance(vulkanInstance, nullptr);
		vulkanInstance = nullptr;
	}
	volkFinalize();

	if (window)
	{
		SDL_DestroyWindow(window);
		window = nullptr;
	}
	SDL_Quit();
}

bool Application::initializeVulkan()
{
	if (!createVulkanInstance())
	{
		std::cerr << "Error creating Vulkan instance" << std::endl;
		return false;
	}

	if (!createVulkanSurface())
	{
		std::cerr << "Error creating Vulkan surface" << std::endl;
		return false;
	}

	if (physicalDevice = findPhysicalDevice(); !physicalDevice)
	{
		std::cerr << "Error finding suitable physical device" << std::endl;
		return false;
	}

	if (!findGraphicsQueue())
	{
		std::cerr << "Error finding graphics queue" << std::endl;
		return false;
	}

	//TODO create logical device
}

bool Application::createVulkanInstance()
{
	if (volkInitialize() != VK_SUCCESS)
	{
		std::cerr << "Failed to initialize Volk" << std::endl;
		return false;
	}

	VkApplicationInfo appInfo
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vulkan 3D Renderer",
		.apiVersion = VulkanVersion,
	};

	uint32_t instExtensionCount = 0;
	const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&instExtensionCount);
	std::vector<const char*> requestedExtensions{ VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
	for (int i = 0; i < instExtensionCount; ++i)
	{
		requestedExtensions.push_back(extensions[i]);
	}

	std::vector<const char*> requestedLayers{ "VK_LAYER_KHRONOS_validation" };

	VkDebugUtilsMessengerCreateInfoEXT debugInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = debugCallback,
	};

	VkInstanceCreateInfo instCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = &debugInfo,
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = static_cast<uint32_t>(requestedLayers.size()),
		.ppEnabledLayerNames = requestedLayers.data(),
		.enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size()),
		.ppEnabledExtensionNames = requestedExtensions.data(),
	};

	if (vkCreateInstance(&instCreateInfo, nullptr, &vulkanInstance) != VK_SUCCESS)
	{
		return false;
	}

	volkLoadInstance(vulkanInstance);
	return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Application::debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		std::cerr << "Vulkan validation layer: " << pCallbackData->pMessage << std::endl;
	}

	return VK_FALSE;
}

bool Application::createVulkanSurface()
{
	if (!SDL_Vulkan_CreateSurface(window, vulkanInstance, nullptr, &surface))
	{
		return false;
	}
	return true;
}

VkPhysicalDevice Application::findPhysicalDevice()
{
	uint32_t physDeviceCount = 0;
	vkEnumeratePhysicalDevices(vulkanInstance, &physDeviceCount, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(physDeviceCount);
	vkEnumeratePhysicalDevices(vulkanInstance, &physDeviceCount, physicalDevices.data());

	VkPhysicalDevice physicalDevice = nullptr;
	if (physDeviceCount)
	{
		physicalDevice = physicalDevices[0];
		for (auto& physicalDevice : physicalDevices)
		{
			VkPhysicalDeviceProperties props{};
			vkGetPhysicalDeviceProperties(physicalDevice, &props);
			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				physicalDevice = physicalDevice;
				break;
			}
		}
	}
	return physicalDevice;
}

bool Application::findGraphicsQueue()
{
	uint32_t queueFamCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamCount, nullptr);
	std::vector<VkQueueFamilyProperties2> queueFamProps(queueFamCount,
		{ .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });
	vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamCount, queueFamProps.data());

	for (int currentFamIdx = 0; currentFamIdx < queueFamProps.size(); ++currentFamIdx)
	{
		VkBool32 hasPresentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, currentFamIdx, surface, &hasPresentSupport);

		const auto& props = queueFamProps[currentFamIdx];
		if (props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && hasPresentSupport)
		{
			gfxQueueFamIdx = currentFamIdx;
			return true;
		}
	}
	return false;
}