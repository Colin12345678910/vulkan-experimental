//> includes
#include "vk_engine.h"
#include "vulkan/vulkan.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

#define VMA_IMPLEMENTATION
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#include "vk_mem_alloc.h"
#include <glm/gtx/transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

using namespace vkutil;

#if NDebug
const bool USE_VALIDATION = false;
#else
const bool USE_VALIDATION = true;
#endif
VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE );

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    InitializeVulkan();

    InitializeSwapchain();

    InitalizeCommands();

    InitializeSyncStructures();

    InitializeDescriptors();

    InitializePipelines();

    InitializeImgui();

    InitializeDefaultData();

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {
        //Wait for the GPU to finish up.
        vkDeviceWaitIdle(_device);
        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);

            _frames[i]._deletionQueue.flush();
        }
        _mainDeletionQueue.flush();

        DestroySwapchain();

        vkDestroyDevice(_device, nullptr);
        vkDestroySurfaceKHR(_instance, _surface, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debugMessager);
        vkDestroyInstance(_instance, nullptr);

        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    //std::this_thread::sleep_for(std::chrono::milliseconds(5));
    //Wait for the gpu to finish rendering the last frame.
    VK_CHECK(vkWaitForFences(_device, 1, &GetCurrentFrame()._renderFence, true, 1000000000));

    GetCurrentFrame()._deletionQueue.flush();
    GetCurrentFrame()._frameDescriptors.ClearPools(_device);
    

    //Get next swapchain image
    uint32_t swapchainImageIndex;
    
    // Acquire next image 
    VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, GetCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR)
    {
        requestResize = true;
    }
    

    //Reset fences.
    VK_CHECK(vkResetFences(_device, 1, &GetCurrentFrame()._renderFence));

    VkCommandBuffer cmd = GetCurrentFrame()._mainCommandBuffer;

    //Reset cmd so we can begin recording
    VK_CHECK(vkResetCommandBuffer(cmd, 0)); //No flags neeeded.


    //Define the CMD
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); //One time use CMD,

    _drawExtent.width = _drawImage.imageExtent.width;
    _drawExtent.height = _drawImage.imageExtent.height;
    //GetDrawExtemt
    _drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * renderScale;
    _drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * renderScale;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //Convert our main drawImage into a layout for writing, and clearing it.
    vkutil::TransitionImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    drawBackground(cmd);

    //Transition to colorAtt optimal bc geometry cannot draw on General.
    vkutil::TransitionImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::TransitionImage(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    DrawGeometry(cmd);

    //Convert the drawimage into a transfer src and the swapchain into a transferDst
    vkutil::TransitionImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::TransitionImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    //copy image from drawImage to swapchain.
    vkutil::CopyImageToImage(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

    //Make the swapchain into a Layout for IMGUI
    vkutil::TransitionImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL); //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

    //let's get IMGUI to draw everything
    DrawImgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    //Make the Swapchain presentable
    vkutil::TransitionImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    //End the CMDBuffer and get ready for exe.
    VK_CHECK(vkEndCommandBuffer(cmd));
    
    //------------------
    // Submit
    // We want to submit our buffer to the queue

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, GetCurrentFrame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame()._renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, GetCurrentFrame()._renderFence));
    //Present the image.
    //We want to present our newly drawn image to the window
    // Which will wait on renderSemaphore to be flagged.

    VkPresentInfoKHR presentInfo = vkinit::present_info();
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &GetCurrentFrame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
    
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        requestResize = true;
    }
    //Increment to the next frame;
    _frameNumber++;
}

void VulkanEngine::drawBackground(VkCommandBuffer cmd)
{
    VkClearColorValue clear;
    float flash = std::abs(std::sin(_frameNumber / 480.f));
    clear = { { 0.0f, 0.0f, flash, 1.0f } };

    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    
    ComputeEffect& effect = backgroundEffects[currentBackground];
    //Bind the gradientPipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    //Bind the descriptorSet
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    //Exec w workgroups of 16
    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        //Calc time
        long lastestframeTime = std::chrono::system_clock::now().time_since_epoch().count();

        long timeForFrame = lastestframeTime - frameTime; //in microseconds
        double framerate = timeForFrame / 10000000.0f;
        framerate = 1.0f / framerate;

        frameTime = lastestframeTime;
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }

            if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDL_KeyCode::SDLK_ESCAPE)
                {
                    bQuit = true;
                }
                fmt::println("Keycode is {}!", std::to_string(e.key.keysym.sym));
            }

            //Send SDL events
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (requestResize)
        {
            ResizeSwapchain();
        }

        //Imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //DebugUI
        //ImGui::ShowDemoWindow();

        if (ImGui::Begin("Background"))
        {
            ImGui::LabelText("Frame", std::to_string(framerate).c_str());
            ImGui::SliderFloat("Render Scale", &renderScale, 0.1f, 1.0f);

            ComputeEffect& selected = backgroundEffects[currentBackground];

            ImGui::Text("Selected effect: ", selected.name);

            ImGui::SliderInt("Effect Index", &currentBackground, 0, backgroundEffects.size() - 1);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);
        }

        ImGui::End();

        //RenerGUI
        ImGui::Render();

        draw();
    }
}

void VulkanEngine::InitializeVulkan()
{
    vkb::InstanceBuilder builder;

    //Make the vulkan instance with basic Debug.
    auto instanceConfig = builder.set_app_name("Example Vulkan Application")
        .request_validation_layers(USE_VALIDATION)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vbInstance = instanceConfig.value();

    _instance = vbInstance.instance;
    _debugMessager = vbInstance.debug_messenger;

    //--------------------------------------------
    // Build the device.

    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    //vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
    features.synchronization2 = true;

    //vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;


    //use vkbootstrap to select a gpu. 
    //We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{ vbInstance };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        .set_required_features_12(features12)
        .set_surface(_surface)
        .select()
        .value();


    //create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the handles to these devices.
    _device = vkbDevice.device;
    _physicalGPU = physicalDevice.physical_device;

    //Get Graphics queue
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    //Create the vulkan allocator.
    VmaAllocatorCreateInfo allocInfo{};
    allocInfo.physicalDevice = _physicalGPU;
    allocInfo.device = _device;
    allocInfo.instance = _instance;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocInfo, &_allocator);

    _mainDeletionQueue.Push([&]() {
        vmaDestroyAllocator(_allocator);
    });
}

void VulkanEngine::InitializeSwapchain()
{
    if (requestResize)
    {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

        vkDestroyImageView(_device, _depthImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);

        //DestroySwapchain();
    }

    CreateSwapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);

    VkExtent3D drawImageExtent =
    {
        w,
        h,
        //std::max(_windowExtent.width, (uint32_t)w),
        //std::max(_windowExtent.height, (uint32_t)h),
        1
    };

    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsage{};
    drawImageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsage |= VK_IMAGE_USAGE_STORAGE_BIT; //Compute shader can write to it
    drawImageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo createImageInfo = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsage, drawImageExtent);

    //We must allocate the image on the GPU
    VmaAllocationCreateInfo vmaImageAllocInfo{};
    vmaImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaImageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(_allocator, &createImageInfo, &vmaImageAllocInfo, &_drawImage.image, &_drawImage.allocation, nullptr));

    //BUild an imgView
    VkImageViewCreateInfo imageViewCreate = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(_device, &imageViewCreate, nullptr, &_drawImage.imageView));

    //Make DepthTexture
    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimgInfo = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);

    //ALlocate and create an image;
    VK_CHECK(vmaCreateImage(_allocator, &dimgInfo, &vmaImageAllocInfo, &_depthImage.image, &_depthImage.allocation, nullptr));

    //Build an imageView
    VkImageViewCreateInfo depthViewInfo = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_device, &depthViewInfo, nullptr, &_depthImage.imageView));

    _mainDeletionQueue.Push([=]() {
        //Overall, this is a hack, like 100% a hack. What we are doing is checking if we've already deallocated the 
        // image and returning if thats the case. This only happens bc we reinit the entire swapchain when we rescale the
        // window. The reason we do that is kinda obvious, but I never want the engine to render a scene weirdly because a player
        // opened it on a smaller monitor and then maximized on a different one.
        // Because of this, we need to push every single new alloc to the deletion queue, however, that image may become stale later

        if (_drawImage.deallocated)
        {
            return;
        }
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

        vkDestroyImageView(_device, _depthImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
        _drawImage.deallocated = true;
    });
}

void VulkanEngine::InitalizeCommands()
{
    //Create a command pool
    //One that can be reset of individual buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        //We want to initialize frameData
        //Create the needed commandPool.
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));
        
        //Allocate a command buffer.
        VkCommandBufferAllocateInfo commandBuffAllocateInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_device, &commandBuffAllocateInfo, &_frames[i]._mainCommandBuffer));
    }

    //----------------------------------
    //Construct Immediate CommandPool & Buffer.
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immediateCommandPool));
    //Alloc a commandBuff for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immediateCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immediateCommandBuffer));

    _mainDeletionQueue.Push([&]() {
        vkDestroyCommandPool(_device, _immediateCommandPool, nullptr);
    });
    //----------------------------------
}

void VulkanEngine::InitializeSyncStructures()
{
    //Creating sync structs

    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT); //Prevents the fence from blocking on first frame.
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore)); //This semaphore is used to prevent GPU from presenting until ready.
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore)); //This will block the GPU while the swapchain is not ready.
    }

    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immediateFence));

    _mainDeletionQueue.Push([&]()
    {
        vkDestroyFence(_device, _immediateFence, nullptr);
    });
}

void VulkanEngine::InitializeDescriptors()
{
    // Create a descriptor pool of size 10, 1
    std::vector<VKDescriptors::DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { 
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 
            1
        }
    };
    globalDescriptiorAllocator.InitPool(_device, 10, sizes);

    {
        VKDescriptors::DescriptorLayoutBuilder builder;
        _drawImageDescriptorLayout = builder
            .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .Build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    //Alloc a descriiptorset for our draw image
    _drawImageDescriptors = globalDescriptiorAllocator.Allocate(_device, _drawImageDescriptorLayout);

    UpdateDescriptors();

    {
        VKDescriptors::DescriptorLayoutBuilder builder;
        _gpuSceneDataDescriptorLayout = builder
            .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .Build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    //Create the descriptor pool that each frame has.
    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        std::vector<VKDescriptors::DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes =
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        _frames[i]._frameDescriptors = VKDescriptors::DescriptorAllocatorGrowable{};
        _frames[i]._frameDescriptors.InitPool(_device, 1000, frameSizes);

        _mainDeletionQueue.Push([&, i]()
        {
            _frames[i]._frameDescriptors.DestroyPool(_device);
        });
    }
    
    VKDescriptors::DescriptorLayoutBuilder builder;
    _singleImageDescriptorLayout = builder
        .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        .Build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);

    _mainDeletionQueue.Push([&]()
    {
        globalDescriptiorAllocator.DestroyPool(_device);
        vkDestroyDescriptorSetLayout(_device, _singleImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
    });
}

void VulkanEngine::UpdateDescriptors()
{
    VKDescriptors::DescriptorWriter writer;
    writer.WriteImage(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    writer.UpdateSet(_device, _drawImageDescriptors);
}

void VulkanEngine::InitializePipelines()
{
    InitializeBackgroundPipelines();
    InitializeMeshPipeline();
}

void VulkanEngine::InitializeBackgroundPipelines()
{
    VkPipelineLayoutCreateInfo computeLayout{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = nullptr };
    //Define the descriptor used for this pipeline.
    computeLayout.setLayoutCount = 1;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(ComputePushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstants;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

    //Create Shaader
    VkShaderModule computeShader;
    VkShaderModule skyShader;
    if (!vkutil::LoadShaderModule("../../shaders/gradient_color.comp.spv", _device, &computeShader))
    {
        fmt::println("Error when building a compute shader");
    }
    if (!vkutil::LoadShaderModule("../../shaders/sky.comp.spv", _device, &skyShader))
    {
        fmt::println("Error when building a compute shader");
    }

    VkPipelineShaderStageCreateInfo stageInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr };
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.pNext = nullptr;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext = nullptr };
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageInfo;

    ComputeEffect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};

    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

    //Change module to create one
    computePipelineCreateInfo.stage.module = skyShader;

    ComputeEffect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};

    //Default params
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    vkDestroyShaderModule(_device, computeShader, nullptr);
    vkDestroyShaderModule(_device, skyShader, nullptr);
    _mainDeletionQueue.Push([=]()
    {
        vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
        vkDestroyPipeline(_device, sky.pipeline, nullptr);
        vkDestroyPipeline(_device, gradient.pipeline, nullptr);
    });
}

void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer)>&& function)
{
    VK_CHECK(vkResetFences(_device, 1, &_immediateFence));
    VK_CHECK(vkResetCommandBuffer(_immediateCommandBuffer, 0));

    VkCommandBuffer cmd = _immediateCommandBuffer;

    VkCommandBufferBeginInfo cmdInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdSubmitInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submitInfo = vkinit::submit_info(&cmdSubmitInfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, _immediateFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immediateFence, VK_TRUE, UINT64_MAX));
}

void VulkanEngine::InitializeImgui()
{
    //This is frankly absurd. I'm not sure why this is recommended?
    VkDescriptorPoolSize pool_sizes[] = { 
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo poolInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = std::size(pool_sizes);
    poolInfo.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;

    VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &imguiPool));

    ImGui::CreateContext();

    ImGui_ImplSDL2_InitForVulkan(_window);

    //Setup initInfo for vulkan
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = _instance;
    initInfo.PhysicalDevice = _physicalGPU;
    initInfo.Device = _device;
    initInfo.Queue = _graphicsQueue;
    initInfo.DescriptorPool = imguiPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.UseDynamicRendering = true;

    //DynRendering
    initInfo.PipelineRenderingCreateInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &_swapchainImageFormat
    };

    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();

    // Add to the deletion queue
    _mainDeletionQueue.Push([=]()
    {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiPool, nullptr);
    });
}
void VulkanEngine::ResizeSwapchain()
{
    vkDeviceWaitIdle(_device);

    DestroySwapchain();

    InitializeSwapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    //CreateSwapchain();
    UpdateDescriptors();
    //CreateSwapchain();
    
    requestResize = false;
}

void VulkanEngine::InitializeMeshPipeline()
{
    VkShaderModule triangleFrag;
    std::string filePath = "../../shaders/tex_image.frag.spv";
    if (!vkutil::LoadShaderModule(filePath.c_str(), _device, &triangleFrag))
    {
        fmt::print("Failed to generate shader ");
        fmt::println("{}", filePath);
    }
    filePath = "../../shaders/colored_triangle_mesh.vert.spv";
    VkShaderModule triangleVert;
    if (!vkutil::LoadShaderModule(filePath.c_str(), _device, &triangleVert))
    {
        fmt::print("Failed to generate shader ");
        fmt::println("{}", filePath);
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayout = vkinit::pipeline_layout_create_info();
    pipelineLayout.pPushConstantRanges = &bufferRange;
    pipelineLayout.pushConstantRangeCount = 1;
    pipelineLayout.pSetLayouts = &_singleImageDescriptorLayout;
    pipelineLayout.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayout, nullptr, &_meshPipelineLayout));

    PipelineBuilder builder;
    _meshPipeline = builder
        .SetShaders(triangleVert, triangleFrag)
        .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
        .SetMultisamplingNone()
        .DisableBlending()
        .EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
        .SetPipelineLayout(_meshPipelineLayout)
        .SetColorAttachmentFormat(_drawImage.imageFormat)
        .SetDepthFormat(_depthImage.imageFormat)
        .BuildPipeline(_device);

    vkDestroyShaderModule(_device, triangleFrag, nullptr);
    vkDestroyShaderModule(_device, triangleVert, nullptr);

    _mainDeletionQueue.Push([&]()
    {
        vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
        vkDestroyPipeline(_device, _meshPipeline, nullptr);
    });
}

void VulkanEngine::InitializeDefaultData()
{
    testMeshes = loadGLTFMeshes(this, "..\\..\\assets\\basicmesh.glb").value();

    _mainDeletionQueue.Push([=]()
    {
        for (auto& mesh : testMeshes)
        {
            DestroyBuffer(mesh->meshBuffers.indexBuffer);
            DestroyBuffer(mesh->meshBuffers.vertexBuffer);
        }
    });

    //Setup default texture
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = CreateImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66, 0.66, 0.66, 1));
    _greyImage = CreateImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));
    _blackImage = CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    //Checkerboard img
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<int, 16 * 16> pixels;
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            int index = y * 16 + x;
            pixels[index] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    _errorImage = CreateImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR; 
    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear);

    _mainDeletionQueue.Push([&]()
    {
        vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
        vkDestroySampler(_device, _defaultSamplerLinear, nullptr);

        DestroyImage(_whiteImage);
        DestroyImage(_greyImage);
        DestroyImage(_blackImage);
        DestroyImage(_errorImage);
    });
}

GPUMeshBuffers VulkanEngine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBuffSize = vertices.size() * sizeof(Vertex);
    const size_t indexBuffSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    //CreateVertexBuff
    newSurface.vertexBuffer = CreateBuffer(vertexBuffSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    //find the Address of vertexBuffer

    VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

    //Create index buffer
    newSurface.indexBuffer = CreateBuffer(indexBuffSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = CreateBuffer(vertexBuffSize + indexBuffSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    //This is all memcpy stuff which in general is unsafe.
    //Cpy vertex buff
    memcpy(data, vertices.data(), vertexBuffSize);

    memcpy((char*)data + vertexBuffSize, indices.data(), indexBuffSize);

    ImmediateSubmit([&](VkCommandBuffer cmd)
    {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBuffSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBuffSize;
        indexCopy.size = indexBuffSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    DestroyBuffer(staging);

    return newSurface;
}

void VulkanEngine::DrawGeometry(VkCommandBuffer cmd)
{
    //Begin setting up GPUSceneData
    //Allocate a new uniform buffer
    AllocatedBuffer gpuSceneDataBuf = CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    //Delete the buff later
    GetCurrentFrame()._deletionQueue.Push([=, this]()
    {
        DestroyBuffer(gpuSceneDataBuf);
    });

    //write the buffer
    GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuf.allocation->GetMappedData();
    *sceneUniformData = _sceneData;

    //Create a descriptorSet that binds the buff
    VkDescriptorSet globalDescriptor = GetCurrentFrame()._frameDescriptors.Allocate(_device, _gpuSceneDataDescriptorLayout);

    VKDescriptors::DescriptorWriter writer;
    writer
        .WriteBuffer(0, gpuSceneDataBuf.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .UpdateSet(_device, globalDescriptor);

    //Begin a render pass to our drawImage
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachmentInfo = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    
    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachmentInfo);

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

    VkViewport viewport{};
    viewport.width = _drawExtent.width;
    viewport.height = _drawExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent.width = _drawExtent.width;
    scissor.extent.height = _drawExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    //Binding a texture
    VkDescriptorSet imageSet = GetCurrentFrame()._frameDescriptors.Allocate(_device, _singleImageDescriptorLayout);
    {
        VKDescriptors::DescriptorWriter writer;
        writer
            .WriteImage(0, _errorImage.imageView, _defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            .UpdateSet(_device, imageSet);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipelineLayout, 0, 1, &imageSet, 0, nullptr);

    GPUDrawPushConstants pushConstants;
    
    //MatrixMath

    glm::mat4 view = glm::translate(glm::vec3{ 0, 0, -5 + (std::sin(_frameNumber / 144.0f) )});

    //Camera
    glm::mat4 proj = glm::perspective(glm::radians(70.0f), (float)_drawExtent.width / (float)_drawExtent.height, 10000.f, 0.1f);

    //Invert the y dir on the projectMatrix
    proj[1][1] *= -1;

    pushConstants.worldMatrix = proj * view;
    //EndMatrixMath
    
    pushConstants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;

    vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

    vkCmdBindIndexBuffer(cmd, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, testMeshes[2]->surfaces[0].count, 1, testMeshes[2]->surfaces[0].startIndex, 0, 0);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr); //No depth for UI

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

AllocatedImage VulkanEngine::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImg;
    newImg.imageFormat = format;
    newImg.imageExtent = size;

    VkImageCreateInfo imgInfo = vkinit::image_create_info(format, usage, size);

    if (mipmapped)
    {
        imgInfo.mipLevels = std::floor(std::log2(std::max(size.width, size.height))) + 1;
    }

    //Setup allocation rules
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create
    VK_CHECK(vmaCreateImage(_allocator, &imgInfo, &allocInfo, &newImg.image, &newImg.allocation, nullptr));

    //If the format is a supported depth format, we need to tell the GPU that we want a depthTex
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
    {
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    //Build the imageview
    VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(format, newImg.image, aspectFlags);
    viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

    VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &newImg.imageView));

    return newImg;
}

AllocatedImage VulkanEngine::CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImg = CreateImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    size_t dataSize = size.depth * size.height * size.width * 4; //Image with the size, height and depth has 4 bytes per pixel;
    AllocatedBuffer uploadBuff = CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadBuff.info.pMappedData, data, dataSize);

    ImmediateSubmit([&](VkCommandBuffer cmd)
    {
        vkutil::TransitionImage(cmd, newImg.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); //Transition our image into one that can be written to.

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        //Copy the buff into the image

        vkCmdCopyBufferToImage(cmd, uploadBuff.buffer, newImg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        vkutil::TransitionImage(cmd, newImg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    DestroyBuffer(uploadBuff);

    return newImg;
}

void VulkanEngine::DestroyImage(const AllocatedImage& img)
{
    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}

AllocatedBuffer VulkanEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .pNext = nullptr };
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaInfo{};
    vmaInfo.usage = memoryUsage;
    vmaInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
    AllocatedBuffer buff;

    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaInfo, &buff.buffer, &buff.allocation, &buff.info));

    return buff;
}

void VulkanEngine::DestroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void VulkanEngine::CreateSwapchain()
{
    vkb::SwapchainBuilder swapchainBuilder{ _physicalGPU, _device, _surface };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR })
        //Vsync present mode.
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(_windowExtent.width, _windowExtent.height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build().value();

    //Store swapchain information.
    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();

    //_mainDeletionQueue.Push([=]() { DestroySwapchain(); });
}

void VulkanEngine::DestroySwapchain()
{
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    for (int i = 0; i < _swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
}