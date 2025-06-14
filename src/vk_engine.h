﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

/* VK Engine	
*	Colin D
*	June 2025
*	Oddly enough, this is kinda hard to fully describe, since at the moment it is the entire rendering engine minus abstractions
*	I do intend to move a lot of the rendering code from general "game" code, but considering I still intend to write this application in a low-level fashion it may be best
*	to describe this as handling the core engine loop.
*/
#pragma once
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <vk_types.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include "vk_images.h"
#include "vk_pipelines.h"

#include <GLTFMetalicRoughness.h>

#include "VkBootstrap.h"


constexpr unsigned int FRAME_OVERLAP = 2;

struct FrameData
{
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	DeletionQueue _deletionQueue;
	VKDescriptors::DescriptorAllocatorGrowable _frameDescriptors;
};

class VulkanEngine {
public:
	FrameData _frames[FRAME_OVERLAP];

	FrameData& GetCurrentFrame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	VkInstance _instance; //Vulkan Library Handle.
	VkDebugUtilsMessengerEXT _debugMessager; //Vulkan debug, (Validation.)
	VkPhysicalDevice _physicalGPU; // Actual gpu we want to select for rendering
	VkDevice _device; // A Vulkan device based on that GPU.
	VkSurfaceKHR _surface; //Vulkan Window surface.

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	AllocatedImage _drawImage;
	AllocatedImage _depthImage;

	VkExtent2D _drawExtent;
	float renderScale = 1.0f;

	long frameTime{ 0 };

	VmaAllocator _allocator;
	
	//VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	//Here's our basic ShaderLayouts
	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	bool requestResize{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };

	DeletionQueue _mainDeletionQueue;

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	VKDescriptors::DescriptorAllocator globalDescriptiorAllocator;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

	VkFence _immediateFence;
	VkCommandBuffer _immediateCommandBuffer;
	VkCommandPool _immediateCommandPool;

	GLTFMetallicRoughness metalRoughMaterial;
	MaterialInstance defaultMaterial;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackground = 0;

	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	void drawBackground(VkCommandBuffer cmd);

	GPUMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	//run main loop
	void run();
private:
	void InitializeVulkan();
	void InitializeSwapchain();
	void InitalizeCommands();
	void InitializeSyncStructures();
	void InitializeDescriptors();
	void UpdateDescriptors();
	void InitializePipelines();
	void InitializeBackgroundPipelines();
	void InitializeMeshPipeline();
	void InitializeDefaultData();
	void ImmediateSubmit(std::function<void(VkCommandBuffer)>&& function);
	void InitializeImgui();
	void ResizeSwapchain();
	void DrawGeometry(VkCommandBuffer cmd);
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

	GPUSceneData _sceneData;
	VkDescriptorSetLayout _singleImageDescriptorLayout;

	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

	//Images
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	void DestroyImage(const AllocatedImage& img);

	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void DestroyBuffer(const AllocatedBuffer& buffer);
	void CreateSwapchain();
	void DestroySwapchain();
};
