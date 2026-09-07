#include "application.hpp"
#include "utils.hpp"

#include <iostream>

bool Application::initialize()
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO))
	{
		window = SDL_CreateWindow("Vulkan 3D Renderer", width, height,  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

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

bool Application::load_data()
{
	// preallocate memory for vertex and index data
	constexpr size_t vertexBufferBytes = 64 * 1024 * 1024; // 64MB vertex budget
	constexpr size_t indexBufferBytes = 32 * 1024 * 1024; // 32MB index budget
	constexpr size_t totalVerts = vertexBufferBytes / sizeof(Vertex);
	constexpr size_t totalIndices = indexBufferBytes / sizeof(uint32_t);
	vertices.resize(totalVerts);
	indices.resize(totalIndices);

	// default white-pixel texture
	uint32_t whitePixelData = 0xFFFFFFFF;
	Image whitePixel
	{
		.width = 1,
		.height = 1,
		.channels = 4,
		.data = reinterpret_cast<unsigned char*>(&whitePixelData),
	};

	VkCommandBuffer whiteImageCmdBuffer = startTransientCommandBuffer();
	auto [whiteImageId, whiteStagingBuffer] = createImage(
		whiteImageCmdBuffer, whitePixel.data, whitePixel.width, whitePixel.height, 4);
	whitePixelImageId = whiteImageId;
	submitTransientCommandBuffer(whiteImageCmdBuffer);
	vmaDestroyBuffer(vmaAllocator, whiteStagingBuffer.vkBuffer, whiteStagingBuffer.allocation);

	VkSamplerCreateInfo samplerInfo
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.compareEnable = VK_FALSE,
	};
	VkSampler sampler = nullptr;
	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
	{
		std::cerr << "Error creating sampler" << std::endl;
		return false;
	}

	samplers.push_back(sampler);
	uint32_t whiteSamplerId = samplers.size();

	textures.push_back(Texture{
		.imageId = whitePixelImageId,
		.samplerId = whiteSamplerId,
		});

	loadGltf(RESOURCES_PATH "models/sea_keep_lonely_watcher/scene.gltf");
}

void Application::loadGltf(const std::string& filepath)
{
	if (!std::filesystem::exists(filepath))
	{
		std::cerr << "File doesn't exist: " << filepath << std::endl;
		return;
	}

	std::cout << " ** Loading GLTF: " << filepath << std::endl;

	tg3_model model;
	tg3_parse_options opts;
	tg3_error_stack errors;

	tg3_parse_options_init(&opts);
	tg3_error_stack_init(&errors);
	tg3_error_code parseResult = tg3_parse_file(&model, &errors, filepath.c_str(), filepath.size(), &opts);

	if (parseResult != TG3_OK)
	{
		std::cerr << "Error parsing glTF file, errors found : \n";
		for (int i = 0; i < errors.count; i++) 
		{
			std::cerr << errors.entries[i].message << "\n";
		}
		std::cerr << std::endl;
		tg3_error_stack_free(&errors);
		return;
	}
	tg3_error_stack_free(&errors);

	std::filesystem::path imageDir = std::filesystem::path(filepath).parent_path();
	std::vector<Image> images = loadImages(model, imageDir);
	std::vector<uint32_t> imageIds = uploadImages(images);

	for (const Image& image : images)
	{
		stbi_image_free(image.data);
	}

	std::vector<uint32_t> samplerIds = loadSamplers(model);
	std::vector<uint32_t> textureIds = loadTextures(model, imageIds, samplerIds);
	std::vector<uint32_t> materialIds = loadMaterials(model, textureIds);
	std::vector<uint32_t> meshIds = loadMeshes(model, materialIds);
}

std::vector<uint32_t> Application::loadSamplers(const tg3_model& model)
{
	std::vector<uint32_t> samplerIds(model.samplers_count);
	for (int i = 0; i < model.samplers_count; i++)
	{
		const tg3_sampler& tg3Sampler = model.samplers[i];
		static const std::unordered_map<uint32_t, std::tuple<VkFilter, VkSamplerMipmapMode, float>> filterMap
		{
			{ TG3_TEXTURE_FILTER_NEAREST,
				{ VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, 0.25f } },
			{ TG3_TEXTURE_FILTER_LINEAR,
				{ VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, 0.25f } },
			{ TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR,
				{ VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_LOD_CLAMP_NONE } },
			{ TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST,
				{ VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_LOD_CLAMP_NONE } },
			{ TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,
				{ VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_LOD_CLAMP_NONE } },
			{ TG3_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST,
				{ VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_LOD_CLAMP_NONE } },
		};
		static const std::unordered_map<uint32_t, VkSamplerAddressMode> wrapMap
		{
			{ TG3_TEXTURE_WRAP_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT },
			{ TG3_TEXTURE_WRAP_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
			{ TG3_TEXTURE_WRAP_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT }
		};

		VkSamplerCreateInfo samplerInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = (tg3Sampler.mag_filter == -1) ? VK_FILTER_LINEAR : std::get<0>(filterMap.at(tg3Sampler.mag_filter)),
			.minFilter = (tg3Sampler.min_filter == -1) ? VK_FILTER_LINEAR : std::get<0>(filterMap.at(tg3Sampler.min_filter)),
			.mipmapMode = (tg3Sampler.min_filter == -1) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : std::get<1>(filterMap.at(tg3Sampler.min_filter)),
			.addressModeU = (tg3Sampler.wrap_s == -1) ? VK_SAMPLER_ADDRESS_MODE_REPEAT : wrapMap.at(tg3Sampler.wrap_s),
			.addressModeV = (tg3Sampler.wrap_t == -1) ? VK_SAMPLER_ADDRESS_MODE_REPEAT : wrapMap.at(tg3Sampler.wrap_t),
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.compareEnable = VK_FALSE,
			.minLod = 0.0f,
			.maxLod = (tg3Sampler.min_filter == -1) ? VK_LOD_CLAMP_NONE : std::get<2>(filterMap.at(tg3Sampler.min_filter))
		};

		VkSampler sampler = nullptr;
		if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
		{
			std::cerr << "Unable to create texture sampler" << std::endl;
			samplerIds[i] = textures[0].samplerId;
		}
		else
		{
			samplers.push_back(sampler);
			samplerIds[i] = samplers.size();
		}
	}
	return samplerIds;
}
std::vector<uint32_t> Application::loadTextures(const tg3_model& model, std::vector<uint32_t>& imageIds, std::vector<uint32_t>& samplerIds)
{

}
std::vector<uint32_t> Application::loadMaterials(const tg3_model& model, std::vector<uint32_t>& textureIds)
{

}
std::vector<uint32_t> Application::loadMeshes(const tg3_model& model, std::vector<uint32_t>& materialIds)
{

}

std::vector<uint32_t> Application::uploadImages(std::vector<Image> images)
{
	VkCommandBuffer commandBuffer = startTransientCommandBuffer();
	std::vector<GPUBuffer> stagingBuffers;
	stagingBuffers.reserve(images.size());

	std::vector<uint32_t> imageIds(images.size());
	for (int i = 0; i < images.size(); i++)
	{
		const Image& image = images[i];
		if (image.data)
		{
			auto [imageId, stagingTexBuffer] = createImage(commandBuffer, image.data, image.width, image.height, 4);
			imageIds[i] = imageId;
			stagingBuffers.push_back(stagingTexBuffer);
		}
		else
		{
			imageIds[i] = whitePixelImageId;
		}
	}

	submitTransientCommandBuffer(commandBuffer);

	for (GPUBuffer& stageBuff : stagingBuffers)
	{
		vmaDestroyBuffer(vmaAllocator, stageBuff.vkBuffer, stageBuff.allocation);
	}
	return imageIds;
}

std::vector<Image> Application::loadImages(const tg3_model &model, const std::filesystem::path &imageDir)
{
	std::vector<Image> images(model.images_count);
	for (int i = 0; i < model.images_count; i++)
	{
		Image& img = images[i];
		std::filesystem::path imagePath = imageDir / model.images[i].uri.data;
		std::cout << "Loading image " << i + 1 << "/" << model.images_count << " : " << model.images[i].uri.data << std::endl;
		img.data = stbi_load(imagePath.string().c_str(), &img.width, &img.height, &img.channels, 4);

		if (!img.data)
		{
			std::cerr << "Failed to load image : " << imagePath.string() << std::endl;
		}
	}

	return images;
}

VkCommandBuffer Application::startTransientCommandBuffer()
{
	VkCommandBufferAllocateInfo cmdAllocInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VkCommandBuffer commandBuffer = nullptr;
	if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS)
	{
		std::cerr << "Error allocating command buffer" << std::endl;
		return nullptr;
	}

	VkCommandBufferBeginInfo cmdBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	if (vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo) != VK_SUCCESS)
	{
		std::cerr << "Error beginning command buffer" << std::endl;
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		return nullptr;
	}

	return commandBuffer;
}

void Application::submitTransientCommandBuffer(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
	};

	vkQueueSubmit(gfxQueue, 1, &submitInfo, nullptr);
	vkQueueWaitIdle(gfxQueue);
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

std::pair<uint32_t, GPUBuffer> Application::createImage(VkCommandBuffer commandBuffer, unsigned char* imageData, uint32_t width, uint32_t height, int channels)
{
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	VkImageCreateInfo imageInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = imageFormat,
		.extent = {.width = width, .height = height, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_AUTO };
	GPUImage gpuImage;
	if (vmaCreateImage(vmaAllocator, &imageInfo, &allocInfo, &gpuImage.image, &gpuImage.allocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Error creating GPU image" << std::endl;
		return { 0, GPUBuffer{} };
	}

	VkImageViewCreateInfo imgViewInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = gpuImage.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = imageFormat,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1,
		}
	};
	if (vkCreateImageView(device, &imgViewInfo, nullptr, &gpuImage.imageView) != VK_SUCCESS)
	{
		std::cerr << "Error creating image view" << std::endl;
		return { 0, GPUBuffer{} };
	}

	VkImageMemoryBarrier2 transferBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
		.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.image = gpuImage.image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};
	VkDependencyInfo transferDepInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &transferBarrier
	};
	vkCmdPipelineBarrier2(commandBuffer, &transferDepInfo);

	const size_t byteSize = width * height * channels;
	GPUBuffer stageBuff = createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, byteSize, true, VMA_MEMORY_USAGE_AUTO_PREFER_HOST);
	mapCopyBufferData(stageBuff, 0, imageData, byteSize);

	VkBufferImageCopy buffImgCopy
	{
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageExtent = {
			.width = width,
			.height = height,
			.depth = 1,
		}
	};
	vkCmdCopyBufferToImage(commandBuffer, stageBuff.vkBuffer, gpuImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffImgCopy);

	VkImageMemoryBarrier2 shaderReadBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
		.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.image = gpuImage.image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};
	VkDependencyInfo shaderReadDepInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &shaderReadBarrier
	};
	vkCmdPipelineBarrier2(commandBuffer, &shaderReadDepInfo);

	images.push_back(gpuImage);
	const uint32_t imageId = images.size();
	return { imageId, stageBuff };
}

GPUBuffer Application::createBuffer(VkBufferUsageFlags usage, size_t byteSize, bool mappable, VmaMemoryUsage memoryUsage)
{
	VkBufferCreateInfo buffInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = byteSize,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo allocInfo
	{
		.flags = mappable ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0u,
		.usage = memoryUsage,
	};
	GPUBuffer gpuBuff;
	if (vmaCreateBuffer(vmaAllocator, &buffInfo, &allocInfo, &gpuBuff.vkBuffer, &gpuBuff.allocation, nullptr) != VK_SUCCESS)
	{
		return GPUBuffer{};
	}

	if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		VkBufferDeviceAddressInfo vertBdaInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = gpuBuff.vkBuffer,
		};
		gpuBuff.deviceAddress = vkGetBufferDeviceAddress(device, &vertBdaInfo);
	}

	return gpuBuff;
}

void Application::mapCopyBufferData(const GPUBuffer& buffer, size_t bufferOffset, void* data, size_t byteSize)
{
	void* buffPtr = nullptr;
	if (vmaMapMemory(vmaAllocator, buffer.allocation, &buffPtr) != VK_SUCCESS)
	{
		std::cerr << "Error mapping buffer memory" << std::endl;
		return;
	}
	std::memcpy(static_cast<char*>(buffPtr) + bufferOffset, data, byteSize);
	vmaUnmapMemory(vmaAllocator, buffer.allocation);
}

void Application::run()
{
	running = true;
	while (running)
	{
		SDL_Event event{0};
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				running = false;
				break;
			}
			else if (event.type == SDL_EVENT_WINDOW_RESIZED)
			{
				width = event.window.data1;
				height = event.window.data2;
				break;
			}
		}

		render();
	}
}

void Application::render()
{
	// Recreate swapchain if needed
	if (requireSwapchainRecreate)
	{
		vkDeviceWaitIdle(device);
		destroySwapchain();
		createSwapchain(width, height);
		requireSwapchainRecreate = false;
	}

	// Semaphore stuff
	const uint32_t frameResIndex = frameIndex++ % MaxFramesInFlight;
	const uint64_t signalValue = nextSignalValue++;
	const uint64_t waitValue = signalValue - MaxFramesInFlight;

	VkSemaphoreWaitInfo waitInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &timelineSemaphore,
		.pValues = &waitValue,
	};
	vkWaitSemaphores(device, &waitInfo, UINT64_MAX);

	FrameResources &res = frameResources[frameResIndex];
	vkResetCommandPool(device, res.commandPool, 0);

	VkSemaphore imageAcquiredSemaphore = res.imageAcquiredSemaphore;

	uint32_t imageIndex = 0;
	VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, &imageIndex);

	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
		requireSwapchainRecreate = true;
		return;
	}
	else if (acquireResult == VK_SUBOPTIMAL_KHR)
	{
		requireSwapchainRecreate = true;
	}

	VkCommandBufferBeginInfo cmdBeginInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(res.commandBuffer, &cmdBeginInfo);

	std::vector<VkImageMemoryBarrier2> layoutBarriers{
		{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		 .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		 .srcAccessMask = 0,
		 .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		 .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		 .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		 .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		 .image = swapchainImages[imageIndex],
		 .subresourceRange{
			 .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			 .baseMipLevel = 0,
			 .levelCount = 1,
			 .baseArrayLayer = 0,
			 .layerCount = 1,
		 }},
		{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		 .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		 .srcAccessMask = 0,
		 .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		 .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		 .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		 .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		 .image = depthImage,
		 .subresourceRange{
			 .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			 .baseMipLevel = 0,
			 .levelCount = 1,
			 .baseArrayLayer = 0,
			 .layerCount = 1,
		 }}};
	VkDependencyInfo depInfo{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = static_cast<uint32_t>(layoutBarriers.size()),
		.pImageMemoryBarriers = layoutBarriers.data(),
	};
	vkCmdPipelineBarrier2(res.commandBuffer, &depInfo);

	VkRenderingAttachmentInfo colorAttachmentInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = swapchainImageViews[imageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue{.color{0.01f, 0.01f, 0.01f, 1.0f}},
	};
	VkRenderingAttachmentInfo depthAttachmentInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = depthImageView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.clearValue{.depthStencil{1.0f, 0}},
	};
	VkRenderingInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea =
			{
				.offset = {.x = 0, .y = 0},
				.extent = {.width = swapchainWidth, .height = swapchainHeight},
			},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentInfo,
		.pDepthAttachment = &depthAttachmentInfo,
	};

	vkCmdBeginRendering(res.commandBuffer, &renderingInfo);

	{
		VkViewport viewport{
			.x = 0,
			.y = 0,
			.width = static_cast<float>(swapchainWidth),
			.height = static_cast<float>(swapchainHeight),
		};
		vkCmdSetViewport(res.commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{
			.offset = {.x = 0, .y = 0},
			.extent = {.width = swapchainWidth, .height = swapchainHeight},
		};
		vkCmdSetScissor(res.commandBuffer, 0, 1, &scissor);

		// draw our triangle
		vkCmdBindPipeline(res.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdDraw(res.commandBuffer, 3, 1, 0, 0);
	}

	vkCmdEndRendering(res.commandBuffer);

	VkImageMemoryBarrier2 presentLayoutBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
		.dstAccessMask = 0,
		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.image = swapchainImages[imageIndex],
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}};
	VkDependencyInfo presentDepInfo{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &presentLayoutBarrier,
	};
	vkCmdPipelineBarrier2(res.commandBuffer, &presentDepInfo);

	vkEndCommandBuffer(res.commandBuffer);

	VkSemaphoreSubmitInfo imageAcquiredWaitInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = imageAcquiredSemaphore,
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	};
	std::vector<VkSemaphoreSubmitInfo> semaphoreSignals{
		{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		 .semaphore = renderCompleteSemaphores[imageIndex],
		 .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT},
		{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		 .semaphore = timelineSemaphore,
		 .value = signalValue,
		 .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}};
	VkCommandBufferSubmitInfo cmdSubmitInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = res.commandBuffer,
	};
	VkSubmitInfo2 submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = &imageAcquiredWaitInfo,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmdSubmitInfo,
		.signalSemaphoreInfoCount = static_cast<uint32_t>(semaphoreSignals.size()),
		.pSignalSemaphoreInfos = semaphoreSignals.data(),
	};
	vkQueueSubmit2(gfxQueue, 1, &submitInfo, VK_NULL_HANDLE);

	VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &renderCompleteSemaphores[imageIndex],
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &imageIndex,
		.pResults = nullptr,
	};

	vkQueuePresentKHR(gfxQueue, &presentInfo);
}

void Application::shutdown()
{
	vkDeviceWaitIdle(device);

	if (timelineSemaphore)
	{
		vkDestroySemaphore(device, timelineSemaphore, nullptr);
		timelineSemaphore = nullptr;
	}

	for (auto &res : frameResources)
	{
		vkDestroySemaphore(device, res.imageAcquiredSemaphore, nullptr);
		vkDestroyCommandPool(device, res.commandPool, nullptr);
	}

	vkDestroyCommandPool(device, commandPool, nullptr);

	if (pipelineLayout)
	{
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		pipelineLayout = nullptr;
	}
	if (pipeline)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = nullptr;
	}

	if (vertexShader)
	{
		vkDestroyShaderModule(device, vertexShader, nullptr);
		vertexShader = nullptr;
	}
	if (fragmentShader)
	{
		vkDestroyShaderModule(device, fragmentShader, nullptr);
		fragmentShader = nullptr;
	}

	destroySwapchain();

	if (vmaAllocator)
	{
		vmaDestroyAllocator(vmaAllocator);
		vmaAllocator = nullptr;
	}

	if (surface)
	{
		vkDestroySurfaceKHR(vulkanInstance, surface, nullptr);
		surface = nullptr;
	}

	if (device)
	{
		vkDestroyDevice(device, nullptr);
		device = nullptr;
	}

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

	if (!createLogicalDevice(physicalDevice))
	{
		std::cerr << "Error creating logical device" << std::endl;
		return false;
	}

	if (!initializeVMA())
	{
		std::cerr << "Error initializing Vulkan Memory Allocator" << std::endl;
		return false;
	}

	if (!createSwapchain(width, height))
	{
		std::cerr << "Error creating swapchain" << std::endl;
		return false;
	}

	if (!createShaders())
	{
		std::cerr << "Error creating shaders" << std::endl;
		return false;
	}

	if (pipeline = createGraphicsPipeline(); !pipeline)
	{
		std::cerr << "Error creating graphics pipeline" << std::endl;
		return false;
	}

	if (!createSyncResources())
	{
		std::cerr << "Error creating synchronization resources" << std::endl;
		return false;
	}

	if (!createCommandBuffer())
	{
		std::cerr << "Error creating command buffer" << std::endl;
		return false;
	}

	return true;
}

bool Application::createVulkanInstance()
{
	if (volkInitialize() != VK_SUCCESS)
	{
		std::cerr << "Failed to initialize Volk" << std::endl;
		return false;
	}

	VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vulkan 3D Renderer",
		.apiVersion = VulkanVersion,
	};

	uint32_t instExtensionCount = 0;
	const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&instExtensionCount);
	std::vector<const char *> requestedExtensions{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
	for (int i = 0; i < instExtensionCount; ++i)
	{
		requestedExtensions.push_back(extensions[i]);
	}

	std::vector<const char *> requestedLayers{"VK_LAYER_KHRONOS_validation"};

	VkDebugUtilsMessengerCreateInfoEXT debugInfo{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = debugCallback,
	};

	VkInstanceCreateInfo instCreateInfo{
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
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData)
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

	std::cout << "Found " << physDeviceCount << " physical devices:" << std::endl;

	VkPhysicalDevice physicalDevice = nullptr;
	if (physDeviceCount)
	{
		physicalDevice = physicalDevices[0];
		for (auto &physicalDevice : physicalDevices)
		{
			VkPhysicalDeviceProperties props{};
			vkGetPhysicalDeviceProperties(physicalDevice, &props);
			std::cout << "- " << props.deviceName << std::endl;
			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				physicalDevice = physicalDevice;
				std::cout << "Using device: " << props.deviceName << std::endl;
				break;
			}
		}
	}

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data());

	bool formatSupported = false;
	for (const VkSurfaceFormatKHR &surfaceFormat : surfaceFormats)
	{
		if (surfaceFormat.format == swapchainFormat)
		{
			formatSupported = true;
			break;
		}
	}
	if (!formatSupported)
	{
		std::cerr << "Required swapchain format not supported by the surface" << std::endl;
		return nullptr;
	}

	return physicalDevice;
}

bool Application::findGraphicsQueue()
{
	uint32_t queueFamCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamCount, nullptr);
	std::vector<VkQueueFamilyProperties2> queueFamProps(queueFamCount,
														{.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2});
	vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamCount, queueFamProps.data());

	for (int currentFamIdx = 0; currentFamIdx < queueFamProps.size(); ++currentFamIdx)
	{
		VkBool32 hasPresentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, currentFamIdx, surface, &hasPresentSupport);

		const auto &props = queueFamProps[currentFamIdx];
		if (props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && hasPresentSupport)
		{
			gfxQueueFamIdx = currentFamIdx;
			return true;
		}
	}
	return false;
}

bool Application::createLogicalDevice(VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceVulkan14Features supportedFeatures14{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES, .pNext = nullptr};
	VkPhysicalDeviceVulkan13Features supportedFeatures13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &supportedFeatures14};
	VkPhysicalDeviceVulkan12Features supportedFeatures12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &supportedFeatures13};
	VkPhysicalDeviceFeatures2 supportedFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &supportedFeatures12};
	vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures);

	if (!supportedFeatures13.dynamicRendering || !supportedFeatures13.synchronization2 ||
		!supportedFeatures12.timelineSemaphore)
	{
		std::cerr << "Required Vulkan features not supported by the physical device" << std::endl;
		return false;
	}

	VkPhysicalDeviceVulkan14Features features14{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
		.pNext = nullptr,
	};
	VkPhysicalDeviceVulkan13Features features13{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &features14,
		.synchronization2 = VK_TRUE,
		.dynamicRendering = VK_TRUE,
	};
	VkPhysicalDeviceVulkan12Features features12{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &features13,
		.timelineSemaphore = VK_TRUE,
	};
	VkPhysicalDeviceFeatures2 features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &features12,
	};

	std::vector<float> queuePriorities{1.0f};
	VkDeviceQueueCreateInfo gfxQueueInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = gfxQueueFamIdx,
		.queueCount = 1,
		.pQueuePriorities = queuePriorities.data(),
	};

	const std::vector<const char *> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	VkDeviceCreateInfo deviceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &gfxQueueInfo,
		.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = nullptr,
	};

	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
	{
		return false;
	}

	vkGetDeviceQueue(device, gfxQueueFamIdx, 0, &gfxQueue);
	if (!gfxQueue)
	{
		std::cerr << "Failed to retrieve graphics queue" << std::endl;
		return false;
	}
	return true;
}

bool Application::initializeVMA()
{
	VmaVulkanFunctions vmaFuncInfo{};
	VmaAllocatorCreateInfo vmaAllocInfo{
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = physicalDevice,
		.device = device,
		.pVulkanFunctions = &vmaFuncInfo,
		.instance = vulkanInstance,
		.vulkanApiVersion = VulkanVersion,
	};

	vmaImportVulkanFunctionsFromVolk(&vmaAllocInfo, &vmaFuncInfo);

	if (vmaCreateAllocator(&vmaAllocInfo, &vmaAllocator) != VK_SUCCESS)
	{
		return false;
	}
	return true;
}

bool Application::createSwapchain(uint32_t width, uint32_t height)
{
	swapchainWidth = width;
	swapchainHeight = height;

	VkSurfaceCapabilitiesKHR surfaceCapabilities{};
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities) != VK_SUCCESS)
	{
		std::cerr << "Failed to get surface capabilities" << std::endl;
		return false;
	}

	uint32_t requestedImageCount = std::max(2u, surfaceCapabilities.minImageCount);
	if (surfaceCapabilities.maxImageCount > 0)
	{
		requestedImageCount = std::min(requestedImageCount, surfaceCapabilities.maxImageCount);
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = requestedImageCount,
		.imageFormat = swapchainFormat,
		.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = {.width = swapchainWidth, .height = swapchainHeight},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = surfaceCapabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
	};

	if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
	{
		std::cerr << "Failed to create swapchain" << std::endl;
		return false;
	}

	uint32_t imageCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
	swapchainImageViews.resize(imageCount);

	for (size_t i = 0; i < swapchainImages.size(); ++i)
	{
		VkImageViewCreateInfo imgViewInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchainFormat,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}};

		if (vkCreateImageView(device, &imgViewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS)
		{
			std::cerr << "Failed to create swapchain image view " << i << std::endl;
			return false;
		}
	}

	renderCompleteSemaphores.resize(swapchainImages.size());
	for (VkSemaphore &semaphore : renderCompleteSemaphores)
	{
		VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
		{
			std::cerr << "Failed to create render-complete semaphore" << std::endl;
			return false;
		}
	}

	VkImageCreateInfo depthCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = depthFormat,
		.extent{.width = swapchainWidth, .height = swapchainHeight, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo allocInfo{
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};
	if (vmaCreateImage(vmaAllocator, &depthCreateInfo, &allocInfo, &depthImage, &depthImageAllocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Failed to create depth image" << std::endl;
		return false;
	}

	VkImageViewCreateInfo depthImgViewInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = depthImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = depthFormat,
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.levelCount = 1,
			.layerCount = 1,
		}};
	if (vkCreateImageView(device, &depthImgViewInfo, nullptr, &depthImageView) != VK_SUCCESS)
	{
		std::cerr << "Failed to create depth image view" << std::endl;
		return false;
	}

	return true;
}

void Application::destroySwapchain()
{
	for (VkImageView swapchainImageView : swapchainImageViews)
	{
		vkDestroyImageView(device, swapchainImageView, nullptr);
	}
	swapchainImageViews.clear();

	for (VkSemaphore semaphore : renderCompleteSemaphores)
	{
		vkDestroySemaphore(device, semaphore, nullptr);
	}
	renderCompleteSemaphores.clear();

	if (swapchain)
	{
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = nullptr;
	}

	if (depthImageView)
	{
		vkDestroyImageView(device, depthImageView, nullptr);
		vmaDestroyImage(vmaAllocator, depthImage, depthImageAllocation);
		depthImageView = nullptr;
	}
}

VkShaderModule Application::createShaderModule(const std::string &fileName, shaderc_shader_kind kind) const
{
	const std::string shaderPath = RESOURCES_PATH "shaders/" + fileName;
	const std::string src = readTextFile(shaderPath);
	if (src.empty())
	{
		std::cerr << "Specified shader file is empty: " << shaderPath << std::endl;
		return nullptr;
	}

	std::cout << "Compiling shader: " << shaderPath << std::endl;
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
	options.SetTargetSpirv(shaderc_spirv_version_1_6);
	options.SetOptimizationLevel(shaderc_optimization_level_performance);
	shaderc::CompilationResult result = compiler.CompileGlslToSpv(src, kind, fileName.c_str(), options);

	if (result.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		std::cerr << "Shader compilation failed: " << result.GetErrorMessage() << std::endl;
		return nullptr;
	}

	const size_t shaderSize = (result.cend() - result.cbegin()) * sizeof(uint32_t);
	VkShaderModuleCreateInfo shaderModuleCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = shaderSize,
		.pCode = result.cbegin(),
	};

	VkShaderModule shaderModule = nullptr;
	if (vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		std::cerr << "Failed to create shader module" << std::endl;
		return nullptr;
	}
	return shaderModule;
}

bool Application::createShaders()
{
	if (vertexShader = createShaderModule("shader.vert", shaderc_vertex_shader); !vertexShader)
	{
		return false;
	}
	if (fragmentShader = createShaderModule("shader.frag", shaderc_fragment_shader); !fragmentShader)
	{
		return false;
	}
	return true;
}

VkPipeline Application::createGraphicsPipeline()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pushConstantRangeCount = 0,
	};

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		std::cerr << "Failed to create pipeline layout" << std::endl;
		return nullptr;
	}

	const char *entryPoint = "main";
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertexShader,
			.pName = entryPoint,
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragmentShader,
			.pName = entryPoint,
		}};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.stencilTestEnable = VK_FALSE,
	};

	VkPipelineViewportStateCreateInfo viewportInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};

	VkPipelineRasterizationStateCreateInfo rasterizationInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo multisampleInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
						  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};
	VkPipelineColorBlendStateCreateInfo colorBlendInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachmentState,
	};

	std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicStateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data(),
	};

	VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &swapchainFormat,
		.depthAttachmentFormat = depthFormat,
	};

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderingInfo,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssemblyInfo,
		.pViewportState = &viewportInfo,
		.pRasterizationState = &rasterizationInfo,
		.pMultisampleState = &multisampleInfo,
		.pDepthStencilState = &depthStencilInfo,
		.pColorBlendState = &colorBlendInfo,
		.pDynamicState = &dynamicStateInfo,
		.layout = pipelineLayout,
		.renderPass = VK_NULL_HANDLE,
	};

	VkPipeline newPipeline = nullptr;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &newPipeline) != VK_SUCCESS)
	{
		std::cerr << "Failed to create graphics pipeline" << std::endl;
		return nullptr;
	}
	return newPipeline;
}

bool Application::createSyncResources()
{
	VkSemaphoreTypeCreateInfo semaphoreInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = MaxFramesInFlight,
	};
	VkSemaphoreCreateInfo semaphoreCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &semaphoreInfo,
	};
	if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &timelineSemaphore) != VK_SUCCESS)
	{
		std::cerr << "Failed to create timeline semaphore" << std::endl;
		return false;
	}

	for (FrameResources &res : frameResources)
	{
		VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &res.imageAcquiredSemaphore) != VK_SUCCESS)
		{
			std::cerr << "Failed to create pre-frame image-acquired semaphore" << std::endl;
			return false;
		}
	}

	return true;
}

bool Application::createCommandBuffer()
{
	VkCommandPoolCreateInfo poolInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = gfxQueueFamIdx,
	};
	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
	{
		std::cerr << "Failed to create command buffer pool" << std::endl;
		return false;
	}

	for (FrameResources &res : frameResources)
	{
		VkCommandPoolCreateInfo poolInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.queueFamilyIndex = gfxQueueFamIdx,
		};
		if (vkCreateCommandPool(device, &poolInfo, nullptr, &res.commandPool) != VK_SUCCESS)
		{
			std::cerr << "Failed to create command buffer pool" << std::endl;
			return false;
		}

		VkCommandBufferAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = res.commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		if (vkAllocateCommandBuffers(device, &allocInfo, &res.commandBuffer) != VK_SUCCESS)
		{
			std::cerr << "Failed to allocate command buffer" << std::endl;
			return false;
		}
	}
	return true;
}