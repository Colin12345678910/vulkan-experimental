#include "stb_image.h"
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(VulkanEngine* engine, std::filesystem::path filepath)
{
	std::cout << "Loading GTLF: " << filepath << std::endl;
	fastgltf::GltfDataBuffer data;
	data.loadFromFile(filepath);

	auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

	fastgltf::Asset gltf;
	fastgltf::Parser parser{};

	auto load = parser.loadBinaryGLTF(&data, filepath.parent_path(), gltfOptions);

	if (load)
	{
		gltf = std::move(load.get());
	}
	else
	{
		fmt::println("Failed to load GLTF: {}", fastgltf::to_underlying(load.error()));
		return {};
	}

	//Load meshes
	std::vector<std::shared_ptr<MeshAsset>> meshes;

	//Use the same vecs for all meshs so we don't hammer the memory
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	for (fastgltf::Mesh& mesh : gltf.meshes)
	{
		MeshAsset newMesh;

		newMesh.name = mesh.name;

		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives)
		{
			GeoSurface newSurface;
			newSurface.startIndex = (uint32_t)indices.size();
			newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

			size_t initialVert = vertices.size();

			//Load Indexes
			{
				fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexAccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, [&](std::uint32_t index)
				{
					indices.push_back(index + initialVert);
				});
			}

			// load vertex positions
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](glm::vec3 v, size_t index)
				{
					Vertex newVert;
					newVert.position = v;
					newVert.normal = { 0, 0, 0 };
					newVert.color = glm::vec4{ 1.f };
					newVert.uv_x = 0;
					newVert.uv_y = 0;
					vertices[initialVert + index] = newVert;
				});
			}

			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
					[&](glm::vec3 v, size_t index)
				{
					vertices[initialVert + index].normal = v;
				});
			}

			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
					[&](glm::vec2 v, size_t index)
				{
					vertices[initialVert + index].uv_x = v.x;
					vertices[initialVert + index].uv_y = v.y;
				});
			}

			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
					[&](glm::vec4 v, size_t index)
				{
					vertices[initialVert + index].color = v;
				});
			}

			newMesh.surfaces.push_back(newSurface);
		}

		//Display VertNormals
		constexpr bool OverrideColors = true;

		if (OverrideColors)
		{
			for (Vertex& vert : vertices)
			{
				vert.color = glm::vec4(vert.normal, 1.0f);
			}
		}
		newMesh.meshBuffers = engine->UploadMesh(indices, vertices);

		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
	}
	return meshes;
}
