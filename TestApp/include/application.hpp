#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <shaderc/shaderc.hpp>
#include <glm/glm.hpp>
#include <tiny_gltf_v3.h>
#include <tinygltf_json.h>
#include <stb_image.h>

#include <vector>
#include <string>
#include <array>
#include <filesystem>

struct FrameResources
{
	VkCommandPool commandPool = nullptr;
	VkCommandBuffer commandBuffer = nullptr;
	VkSemaphore imageAcquiredSemaphore = nullptr;
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 normal;
	glm::vec2 uv;
};

struct Image
{
	int width;
	int height;
	int channels;
	unsigned char* data;
};

struct GPUImage
{
	VkImage image = nullptr;
	VkImageView imageView = nullptr;
	VmaAllocation allocation = nullptr;
};

struct GPUBuffer
{
	VkBuffer vkBuffer = nullptr;
	uint64_t deviceAddress = 0;
	VmaAllocation allocation = nullptr;
};

struct Texture
{
	uint32_t imageId = 0;
	uint32_t samplerId = 0;
};

class Application {
	constexpr static uint32_t VulkanVersion{ VK_API_VERSION_1_4 };
	constexpr static VkFormat swapchainFormat{ VK_FORMAT_B8G8R8A8_SRGB };
	constexpr static VkFormat depthFormat{ VK_FORMAT_D32_SFLOAT };
	constexpr static uint32_t MaxFramesInFlight{ 2 };

public:
	bool initialize();
	bool load_data();
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
	VkCommandBuffer startTransientCommandBuffer();
	void submitTransientCommandBuffer(VkCommandBuffer commandBuffer);

	std::pair<uint32_t, GPUBuffer> createImage(VkCommandBuffer commandBuffer, unsigned char* imageData, uint32_t width, uint32_t height, int channels);
	GPUBuffer createBuffer(VkBufferUsageFlags usage, size_t byteSize, bool mappable, VmaMemoryUsage memoryUsage);
	void mapCopyBufferData(const GPUBuffer& buffer, size_t bufferOffset, void* data, size_t byteSize);

	void loadGltf(const std::string& filepath);
	std::vector<Image> loadImages(const tg3_model &model, const std::filesystem::path &imageDir);
	std::vector<uint32_t> uploadImages(std::vector<Image> images);
	std::vector<uint32_t> loadSamplers(const tg3_model& model);
	std::vector<uint32_t> loadTextures(const tg3_model& model, std::vector<uint32_t>& imageIds, std::vector<uint32_t>& samplerIds);
	std::vector<uint32_t> loadMaterials(const tg3_model& model, std::vector<uint32_t>& textureIds);
	std::vector<uint32_t> loadMeshes(const tg3_model& model, std::vector<uint32_t>& materialIds);

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
	std::vector<VkImage> swapchainImages{};
	std::vector<VkImageView> swapchainImageViews{};
	std::vector<VkSemaphore> renderCompleteSemaphores{};

	VkImage depthImage = nullptr;
	VkImageView depthImageView = nullptr;
	VmaAllocation depthImageAllocation = nullptr;

	VkShaderModule vertexShader = nullptr;
	VkShaderModule fragmentShader = nullptr;

	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;

	VkSemaphore timelineSemaphore = nullptr;
	std::array<FrameResources, MaxFramesInFlight> frameResources{};

	bool requireSwapchainRecreate = true;
	uint64_t frameIndex = MaxFramesInFlight;
	uint64_t nextSignalValue = MaxFramesInFlight + 1;

	std::vector<Vertex> vertices{};
	std::vector<uint32_t> indices{};

	VkCommandPool commandPool = nullptr;

	uint32_t whitePixelImageId = 0;
	uint32_t vertexBufferId = 0;
	uint32_t indexBufferId = 0;
	uint32_t materialBufferId = 0;
	std::vector<GPUImage> images{};
	std::vector<VkSampler> samplers{};
	std::vector<Texture> textures{};
};