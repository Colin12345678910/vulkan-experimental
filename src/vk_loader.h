/* VK_Loader
*	Colin D
*	June 2025
*	Loads up a GLTF mesh and loads it into a meshAsset, including both a mesh and a surface (which just defines a starting point
*	and count of indices present within the mesh.
*/
#pragma once
#include <RenderNode.h>
#include <vk_descriptors.h>
#include <unordered_map>
#include <filesystem>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

//struct GLTFMaterial
//{
//	MaterialInstance data;
//};
//struct GeoSurface
//{
//	uint32_t startIndex;
//	uint32_t count;
//	std::shared_ptr<GLTFMaterial> material;
//};

//struct MeshAsset
//{
//	std::string name;
//
//	std::vector<GeoSurface> surfaces;
//	GPUMeshBuffers meshBuffers;
//};

//forward Declaration
class VulkanEngine;
struct LoadedGLTF : public RenderNode
{
	std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
	std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;
	std::unordered_map<std::string, std::shared_ptr<RenderNode>> renderNodes;
	std::unordered_map<std::string, AllocatedImage> images;

	std::vector<std::shared_ptr<RenderNode>> topNodes;
	std::vector<VkSampler> samplers;

	VKDescriptors::DescriptorAllocatorGrowable descriptorPool;

	AllocatedBuffer materialDataBuffer;
	VulkanEngine* engine = nullptr;

	virtual void Draw(const glm::mat4& topMat, DrawContext& ctx);

	~LoadedGLTF() { ClearAll(); }
private:
	void ClearAll();
};

std::optional<AllocatedImage> loadImage(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);
std::optional<std::shared_ptr<LoadedGLTF>> loadGLTF(VulkanEngine* engine, std::filesystem::path filepath);
std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(VulkanEngine* engine, std::filesystem::path filepath);