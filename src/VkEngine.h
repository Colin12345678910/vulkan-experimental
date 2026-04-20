// vulkan_guide.h : Include file for standard system include files,
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
#include <stb_image.h>

#include <VkTypes.h>
#include <VkDescriptors.h>
#include <VkLoader.h>
#include <unordered_set>
#include "VkImages.h"
#include "VkPipelines.h"
#include "RenderPipeline.h"
#include <GLTFMetalicRoughness.h>

#include "Camera.h"
#include <VkBootstrap.h>
#include "RenderNode.h"
#include "ShadowMap.h"
#include "HDRI.h"
#include <forward_list>

constexpr unsigned int FRAME_OVERLAP = 3;

struct FrameData
{
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;
	VkFence _shadowFence;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	DeletionQueue _deletionQueue;
	VKDescriptors::DescriptorAllocatorGrowable _frameDescriptors;
};

struct ExitInstructions
{
	bool relaunch{ false };
	std::string scenePath;
};

struct EngineStats
{
	double frameTime{ 0.0f };
	int64_t triangleCount{ 0 };
	int64_t drawCalls{ 0 };
	double sceneUpdateTime{ 0.0f };
	double meshDrawTime{ 0.0f };
	int64_t transparents{ 0 };

	EngineStats operator+(const EngineStats& other)
	{
		EngineStats result;
		result.frameTime = frameTime + other.frameTime;
		result.triangleCount = triangleCount + other.triangleCount;
		result.drawCalls = drawCalls + other.drawCalls;
		result.sceneUpdateTime = sceneUpdateTime + other.sceneUpdateTime;
		result.meshDrawTime = meshDrawTime + other.meshDrawTime;
		result.transparents = transparents + other.transparents;
		return result;
	}

	EngineStats operator/(double divisor)
	{
		EngineStats result;
		result.frameTime = frameTime / divisor;
		result.triangleCount = triangleCount / divisor;
		result.drawCalls = drawCalls / divisor;
		result.sceneUpdateTime = sceneUpdateTime / divisor;
		result.meshDrawTime = meshDrawTime / divisor;
		result.transparents = transparents / divisor;
		return result;
	}

	void Print()
	{
		fmt::println("Frame Time: {} ms", frameTime);
		fmt::println("FPS: {}", 1000.0f / frameTime);
		fmt::println("Draw time: {} ms", meshDrawTime);
		fmt::println("Update Time: {} ms", sceneUpdateTime);
		fmt::println("Triangles: {}", triangleCount);
		fmt::println("Drawcalls: {}", drawCalls);
		fmt::println("Transparents: {}", transparents);
	}
};

static std::forward_list<EngineStats> frameStats;

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

	//DrawContext
	DrawContext mainDrawCtx;
	std::unordered_map < std::string, std::shared_ptr<RenderNode>> loadedNodes;
	
	

	//Here's our basic ShaderLayouts
	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;

	Camera _camera;

	bool _isInitialized{ false };
	int _frameNumber {0};
	long long time{ 0 };
	bool stop_rendering{ false };
	bool requestResize{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };

	DeletionQueue _mainDeletionQueue;

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	VKDescriptors::DescriptorAllocatorGrowable globalDescriptiorAllocator;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

	VkFence _immediateFence;
	VkCommandBuffer _immediateCommandBuffer;
	VkCommandPool _immediateCommandPool;

	GLTFMetallicRoughness metalRoughMaterial;
	MaterialInstance defaultMaterial;
	GPUSceneData _sceneData;

	EngineStats stats;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackground = 0;

	ExitInstructions exitInstructions;

	//std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	//initializes everything in the engine
	void init(ExitInstructions instructions);
	void init() { init({ ExitInstructions{} }); };

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	std::vector<std::function<void()>> _drawList;

	double DeltaTime() { return stats.frameTime; }

	GPUMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	AllocatedImage GetDefaultImage() { return _errorImage; }
	AllocatedImage GetWhiteImage() { return _blackImage; }
	VkSampler GetDefaultSampler(bool linear = true) { return linear ? _defaultSamplerLinear : _defaultSamplerNearest; }
	HDRI GetCurrentHDRI() { return hdri; }

	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, int mipLevels = 1, std::string name = "VulkanEngine::CreateImage", int arrayLayers = 1);
	AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, std::string name = "VulkanEngine::CreateImage", int arrayLayers = 1);
	AllocatedImage CreateMippedImage(std::vector<std::vector<uint8_t>> data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, std::string name = "VulkanEngine::CreateMippedImage");
	AllocatedImage CreateCubeImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, std::string name = "VulkanEngine::CreateCubeImage");
	AllocatedImage CreateCubeImage(const std::vector<std::vector<char>>& data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, std::string name);
	AllocatedImage CopyDataToImage(const void* data, AllocatedImage img, int mip = 0, int face = 0, uint32_t dataSize = 0);

	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, std::string name = "VulkanEngine::Internal::Staging");
	void DestroyBuffer(const AllocatedBuffer& buffer);
	void DestroyImage(const AllocatedImage& img);
	void FlushDrawCtx(VkCommandBuffer cmd, VkDescriptorSet& globalDescriptor);
	void FlushDrawCtx(VkCommandBuffer cmd, VkDescriptorSet& globalDescriptor, VkPipeline pipelineOverride, VkPipelineLayout pipelineLayoutOverride);

	
	void ImmediateSubmit(std::function<void(VkCommandBuffer)>&& function);
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
	void InitializeDefaultImages();
	void Initialize3DNoise(std::string root);
	void InitializeDefaultData();
	void InitializeImgui();
	void ResizeSwapchain();
	void DrawGeometry(VkCommandBuffer cmd);
	void DrawShadows(VkCommandBuffer cmd);
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void UpdateScene();

	VkDescriptorSetLayout _singleImageDescriptorLayout;

	//Images
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorImage;

	AllocatedImage _noiseImage;
	AllocatedBuffer screenshotBuffer;

	RenderPipeline _rendergraph;

	HDRI hdri;

	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	std::vector<IGeometryPass*> geometryPasses;

	ShadowMap shadowMap;
	void CreateSwapchain();
	void DestroySwapchain();
	bool IsVisible(const RenderObject& obj, const glm::mat4& viewProj);
};
