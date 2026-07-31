#pragma once

#include <Volk/volk.h>

class Application {
	constexpr static uint32_t VulkanVersion{ VK_API_VERSION_1_4 };

public:
	bool initialize();
	void run();
	void shutdown();

private:
	bool initializeVulkan();
	bool createVulkanInstance();
	bool createVulkanSurface();
	VkPhysicalDevice findPhysicalDevice();
	bool findGraphicsQueue();

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	SDL_Window* window = nullptr;
	VkInstance vulkanInstance = nullptr;
	VkSurfaceKHR surface = nullptr;
	VkPhysicalDevice physicalDevice = nullptr;
	uint32_t gfxQueueFamIdx = UINT32_MAX;
	VkQueue gfxQueue = nullptr;
	uint32_t width = 800;
	uint32_t height = 600;
};