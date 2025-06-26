/* VK_Types
*	Colin D
*	June 2025
*	This is just a collection of types into a centralized location.
*/
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
//class VKDescriptors::DescriptorAllocatorGrowable;

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

struct DeletionQueue
{
    std::deque<std::function<void()>> toDelete;

    void Push(std::function<void()>&& function)
    {
        toDelete.push_back(function);
    }

    void flush()
    {
        for (auto func = toDelete.rbegin(); func != toDelete.rend(); func++)
        {
            (*func)();
        }
        toDelete.clear();
    }
};
struct AllocatedImage
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
    bool deallocated = false;
};
struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};
struct ComputePushConstants
{
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};
struct ComputeEffect
{
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};
struct Vertex
{
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};
struct GPUMeshBuffers
{
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};
struct GPUDrawPushConstants
{
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};
struct GPUSceneData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColor;
};
struct MaterialPipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};
enum class MaterialPass : uint8_t
{
    MainColor,
    Transparent,
    Other
};
/// <summary>
/// An instance of a Material, used as a larger component of a Render Object anda draw context.
/// </summary>
struct MaterialInstance
{
    //The pipeline associated with the Material, this is typically a normal PBR pipeline.
    MaterialPipeline* pipeline;
    //The DescriptorSet for this material, this is written typically by whatever class handles the Material
    VkDescriptorSet materialSet;
    //Describes the render pass used by the Material, used for Transparency effects.
    MaterialPass passType; 
};
struct RenderObject
{
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

struct DrawContext
{
    std::vector<RenderObject> OpaqueSurfaces;
};