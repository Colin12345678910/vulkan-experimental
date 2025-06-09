/* VK_Loader
*	Colin D
*	June 2025
*	Loads up a GLTF mesh and loads it into a meshAsset, including both a mesh and a surface (which just defines a starting point
*	and count of indices present within the mesh.
*/
#pragma once
#include <vk_types.h>
#include <unordered_map>
#include <filesystem>
struct GeoSurface
{
	uint32_t startIndex;
	uint32_t count;
};

struct MeshAsset
{
	std::string name;

	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};

//forward Declaration
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(VulkanEngine* engine, std::filesystem::path filepath);