#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <shaderc/shaderc.hpp>

#include <vector>
#include <string>
#include <array>

struct FrameResources
{
	VkCommandPool commandPool = nullptr;
	VkCommandBuffer commandBuffer = nullptr;
	VkSemaphore imageAcquiredSemaphore = nullptr;
};

class Application {
	constexpr static uint32_t VulkanVersion{ VK_API_VERSION_1_4 };
	constexpr static VkFormat swapchainFormat{ VK_FORMAT_B8G8R8A8_SRGB };
	constexpr static VkFormat depthFormat{ VK_FORMAT_D32_SFLOAT };
	constexpr static uint32_t MaxFramesInFlight{ 2 };

public:
	bool initialize();
	void run();
	void shutdown();

private:
	bool running = false;
	bool initializeVulkan();
	bool createVulkanInstance();
	bool createVulkanSurface();
	VkPhysicalDevice findPhysicalDevice();
	bool findGraphicsQueue();
	bool createLogicalDevice(VkPhysicalDevice physicalDevice);
	bool initializeVMA();
	bool createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain();
	VkShaderModule createShaderModule(const std::string &fileName, shaderc_shader_kind kind) const;
	bool createShaders();
	VkPipeline createGraphicsPipeline();
	bool createSyncResources();
	bool createCommandBuffer();

	void render();

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	SDL_Window* window = nullptr;
	uint32_t width = 800;
	uint32_t height = 600;

	VkInstance vulkanInstance = nullptr;
	VkPhysicalDevice physicalDevice = nullptr;
	VkDevice device = nullptr;
	VkSurfaceKHR surface = nullptr;
	VmaAllocator vmaAllocator = nullptr;

	uint32_t gfxQueueFamIdx = UINT32_MAX;
	VkQueue gfxQueue = nullptr;

	uint32_t swapchainWidth = 0;
	uint32_t swapchainHeight = 0;
	VkSwapchainKHR swapchain = nullptr;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkSemaphore> renderCompleteSemaphores;

	VkImage depthImage = nullptr;
	VkImageView depthImageView = nullptr;
	VmaAllocation depthImageAllocation = nullptr;

	VkShaderModule vertexShader = nullptr;
	VkShaderModule fragmentShader = nullptr;

	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;

	VkSemaphore timelineSemaphore = nullptr;
	std::array<FrameResources, MaxFramesInFlight> frameResources;

	bool requireSwapchainRecreate;
	uint64_t frameIndex = MaxFramesInFlight;
	uint64_t nextSignalValue = MaxFramesInFlight + 1;
};