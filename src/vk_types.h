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

#include "CVars.h" //Put in pch?

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
	glm::vec4 cameraForward;
	glm::mat4 viewProj;
    glm::mat4 invProj;
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
	glm::vec3 tangent;
	float padding; //Align to 16 bytes
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
    glm::mat4 shadowCoord;
    glm::vec4 cameraPos;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColor;
	glm::vec4 time;
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
	Cutout,
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
struct GLTFMaterial
{
    MaterialInstance data;
};
struct Bounds
{
    glm::vec3 origin;
    float radius;
    glm::vec3 extents;
};
struct GeoSurface
{
    uint32_t startIndex;
    uint32_t count;
	Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
};
struct MeshAsset
{
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};
struct RenderObject
{
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;

	Bounds bounds;
    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

struct DrawContext
{
    std::vector<RenderObject> OpaqueSurfaces;
	std::vector<RenderObject> TransparentSurfaces;
};