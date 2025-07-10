#include "stb_image.h"
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

int fPath = 0, Vector = 0, BView = 0, Meshes = 0, Samplers = 0, Materials = 0;

void LoadedGLTF::ClearAll()
{
	fmt::println("Clearing GLTF data...");
	VkDevice device = engine->_device;
	
	descriptorPool.DestroyPool(device); //Destroy the descriptor pool
	engine->DestroyBuffer(materialDataBuffer); //Destroy the material data buffer

	for (auto& [k, mesh] : meshes)
	{
		engine->DestroyBuffer(mesh->meshBuffers.vertexBuffer); //Destroy the vertex buffer
		engine->DestroyBuffer(mesh->meshBuffers.indexBuffer); //Destroy the index buffer
	}
	for (auto& [k, img] : images)
	{
		if (img.image == engine->GetDefaultImage().image) //If the image is the default image, skip it
			continue;
		engine->DestroyImage(img); //Destroy the images
	}

	for (auto& sampler : samplers)
	{
		vkDestroySampler(device, sampler, nullptr); //Destroy the samplers
	}	
}

VkFilter ExtractFilter(fastgltf::Filter filter)
{
	switch (filter)
	{
		case fastgltf::Filter::Nearest: 
		case fastgltf::Filter::NearestMipMapLinear:
		case fastgltf::Filter::NearestMipMapNearest:
			return VK_FILTER_NEAREST;

		case fastgltf::Filter::Linear:
		case fastgltf::Filter::LinearMipMapNearest:
		case fastgltf::Filter::LinearMipMapLinear:
			return VK_FILTER_LINEAR;
		default:
			fmt::println("Unknown filter type: {}", fastgltf::to_underlying(filter));
			return VK_FILTER_LINEAR;
	}
	return VkFilter();
}

VkSamplerMipmapMode ExtractMipmapMode(fastgltf::Filter filter)
{
	switch (filter)
	{
	case fastgltf::Filter::Nearest:
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::NearestMipMapLinear:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;

	case fastgltf::Filter::Linear:
	case fastgltf::Filter::LinearMipMapNearest:
	case fastgltf::Filter::LinearMipMapLinear:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;

	default:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}
}

void LoadedGLTF::Draw (const glm::mat4& topMat, DrawContext& ctx)
{
	// Draw all the render nodes in the scene
	for(auto& n : topNodes)
	{
		n->Draw(topMat, ctx);
	}
}

std::optional<AllocatedImage> loadImage(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
	AllocatedImage img{};

	int width, height, channels;
	if (image.name == "GTLF_0010_tex49")
	{
		fmt::println("Image: {}", image.name);
	}

	std::visit(
		fastgltf::visitor
		{
			[](auto& arg) {}, //Unsupported
			[&](fastgltf::sources::URI& filePath)
			{
				fPath++;
				assert(filePath.fileByteOffset == 0); //We don't support offsets with
				assert(filePath.uri.isLocalPath()); //We cannot load remote images

				const std::string root("../../assets/");
				std::string path(filePath.uri.path().begin(), filePath.uri.path().end());

				path = root + path; //Prepend the root path to the image path

				unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

				if (data)
				{
					VkExtent3D imageSize;
					imageSize.width = width;
					imageSize.height = height;
					imageSize.depth = 1;

					img = engine->CreateImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false, std::string("GLTF::URI::" + image.name));
					stbi_image_free(data); //Free the image data after we are done with it
				}
				else
				{
					fmt::println("Failed to load image: {}", path);
				}
			},
			[&](fastgltf::sources::Vector& vector)
			{
				Vector++;
				unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width, &height, &channels, 4);

				if (data)
				{
					VkExtent3D imageSize;
					imageSize.width = width;
					imageSize.height = height;
					imageSize.depth = 1;
					img = engine->CreateImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false, std::string("GLTF::VEC::" + image.name));
					stbi_image_free(data); //Free the image data after we are done with it
				}
			},
			[&](fastgltf::sources::BufferView& view)
			{
				BView++;	
				auto& bufferView = asset.bufferViews[view.bufferViewIndex];
				auto& buffer = asset.buffers[bufferView.bufferIndex];

				std::visit(fastgltf::visitor{
					[](auto& arg) {},
					[&](fastgltf::sources::Vector& vector)
					{
						unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
							static_cast<int>(bufferView.byteLength),
							&width, &height, &channels, 4
							);
						if (data)
						{
							VkExtent3D imageSize;
							imageSize.width = width;
							imageSize.height = height;
							imageSize.depth = 1;
							img = engine->CreateImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false, std::string("GLTF::BUFV::" + image.name));
							stbi_image_free(data); //Free the image data after we are done with it
						}
					}
					}, buffer.data );
			},
		},
	image.data
	);

	// Failed to load image
	if (img.image == VK_NULL_HANDLE)
	{
		//fmt::println("Failed to load image: {}", image.name);
		return std::nullopt; //Return empty optional
	}
	else
	{
		return img; //Return the allocated image
	}
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGLTF(VulkanEngine* engine, std::filesystem::path filepath)
{
	fmt::println("Loading GLTF: {}", filepath.string());

	std::shared_ptr<LoadedGLTF> loadedScene = std::make_shared<LoadedGLTF>();

	loadedScene->engine = engine;

	LoadedGLTF& file = *loadedScene.get();

	fastgltf::Parser parser;

	constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::AllowDouble | fastgltf::Options::DontRequireValidAssetMember;

	fastgltf::GltfDataBuffer data;
	data.loadFromFile(filepath);

	fastgltf::Asset gltf;

	std::filesystem::path path = filepath;

	auto type = fastgltf::determineGltfFileType(&data);

	switch (type)
	{
		case fastgltf::GltfType::glTF:
		{
			auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
			if (load)
			{
				gltf = std::move(load.get());
			}
			else
			{
				fmt::println("Failed to load GLTF: {}", fastgltf::to_underlying(load.error()));
				return {};
			}
			break;
		}
		case fastgltf::GltfType::GLB:
		{
			auto loadBin = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
			if (loadBin)
			{
				gltf = std::move(loadBin.get());
			}
			else
			{
				fmt::println("Failed to load GLB: {}", fastgltf::to_underlying(loadBin.error()));
				return {};
			}
			break;
		}
		case fastgltf::GltfType::Invalid:
			fmt::println("Invalid GLTF file type: {}", filepath.string());
			return {};
	}

	std::vector<VKDescriptors::DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { 
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 }, //Our three images for the material
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 }, //
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
	};

	file.descriptorPool.InitPool(engine->_device, gltf.materials.size(), sizes);

	//load samplers
	for (fastgltf::Sampler& sampler : gltf.samplers)
	{
		Samplers++;
		VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(
			ExtractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest)),
			ExtractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
			ExtractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest))
			);

		VkSampler sampler;
		vkCreateSampler(engine->_device, &samplerInfo, nullptr, &sampler);

		file.samplers.push_back(sampler);
	}

	//Temportary storage for objs
	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<std::shared_ptr<RenderNode>> renderNodes;
	std::vector<AllocatedImage> images;
	std::vector<std::shared_ptr<GLTFMaterial>> materials;

	//Load textures
	for (fastgltf::Image& img : gltf.images)
	{
		if (file.images.count(img.name.c_str())) //If the image is already loaded, skip it
		{
			fmt::println("Image {} already loaded, skipping.", img.name);
			images.push_back(engine->GetDefaultImage());
			continue;
		}
		std::optional<AllocatedImage> image = loadImage(engine, gltf, img); //Load the image

		if (image.has_value())
		{
			images.push_back(image.value()); //Push the image to the vector
			file.images[img.name.c_str()] = image.value(); //Push the image into the map
		}
		else
		{
			//fmt::println("Failed to load image: {}", img.name);
			images.push_back(engine->GetDefaultImage());
			//return {}; //Return empty optional if we failed to load the image
		}
	}

	file.materialDataBuffer = engine->CreateBuffer(sizeof(GLTFMetallicRoughness::MaterialConstants) * gltf.materials.size(), //Create GPUBuff
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	int dataIndex = 0;
	GLTFMetallicRoughness::MaterialConstants* sceneData = (GLTFMetallicRoughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData; //Map the buffer to CPU
	

	//Loading materials
	for (fastgltf::Material& mat : gltf.materials)
	{
		Materials++;
		std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
		materials.push_back(newMaterial);
		file.materials[mat.name.c_str()] = newMaterial; //Push mat into the map

		GLTFMetallicRoughness::MaterialConstants matData = {};
		matData.colorFactors.x = mat.pbrData.baseColorFactor[0]; //R
		matData.colorFactors.y = mat.pbrData.baseColorFactor[1]; //G
		matData.colorFactors.z = mat.pbrData.baseColorFactor[2]; //B
		matData.colorFactors.w = mat.pbrData.baseColorFactor[3]; //A

		matData.metalRoughFactors.x = mat.pbrData.metallicFactor; //Metallic
		matData.metalRoughFactors.y = mat.pbrData.roughnessFactor; //Roughness

		sceneData[dataIndex] = matData; //Copy data to the mapped buffer

		MaterialPass passType = MaterialPass::MainColor;

		if (mat.alphaMode == fastgltf::AlphaMode::Blend)
		{
			passType = MaterialPass::Transparent;
		}
		//TODO : Support more alpha modes
		else if (mat.alphaMode == fastgltf::AlphaMode::Mask)
		{
			fmt::println("Alpha mode mask not supported yet, using main color pass for now.");
		}

		// Setting up default resources
		GLTFMetallicRoughness::MaterialResources resources;
		resources.colorImage = engine->GetDefaultImage(); //Default image for now
		resources.colorSampler = engine->GetDefaultSampler(); //Default sampler for now
		resources.metalRoughImage = engine->GetDefaultImage(); //Default image for now

		//Uniform buffer for the material
		resources.dataBuffer = file.materialDataBuffer.buffer;
		resources.dataBufferOffset = dataIndex * sizeof(GLTFMetallicRoughness::MaterialConstants);

		//Grab textures
		if (mat.pbrData.baseColorTexture.has_value())
		{
			size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value(); //Image ineex
			size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value(); //Sampler index

			resources.colorImage = images[img]; //Get the image
			resources.colorSampler = file.samplers[sampler]; //Get the sampler
		}

		//Build
		newMaterial->data = engine->metalRoughMaterial.WriteMaterial(engine->_device, passType, resources, file.descriptorPool);

		dataIndex++; //Increment the index for the next material
	}

	//Load meshes
	{
		Meshes++;
		std::vector<uint32_t> indices; //we will use the same vecs for all meshes so we don't hammer the memory
		std::vector<Vertex> vertices;

		for (fastgltf::Mesh& mesh : gltf.meshes)
		{
			std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
			meshes.push_back(newMesh);
			file.meshes[mesh.name.c_str()] = newMesh; //Push mesh into the map
			newMesh->name = mesh.name;

			//Clear the vectors for the new mesh
			indices.clear();
			vertices.clear();

			for (auto& primitive : mesh.primitives)
			{
				GeoSurface newSurface;
				newSurface.startIndex = (uint32_t)indices.size();
				newSurface.count = (uint32_t)gltf.accessors[primitive.indicesAccessor.value()].count;
				size_t initialVert = vertices.size();
				//Load Indexes
				{
					fastgltf::Accessor& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
					indices.reserve(indices.size() + indexAccessor.count);
					fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, [&](std::uint32_t index)
					{
						indices.push_back(index + initialVert);
					});
				}
				// load vertex positions
				{
					fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->second];
					vertices.resize(vertices.size() + posAccessor.count);
					fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](glm::vec3 v, size_t index)
					{
						Vertex newVert;
						newVert.position = v;
						newVert.normal = { 1.0, 0, 0 };
						newVert.color = glm::vec4{ 1.f };
						newVert.uv_x = 0;
						newVert.uv_y = 0;
						vertices[initialVert + index] = newVert;
					});
				}
				// load vertex normals
				{
					auto normals = primitive.findAttribute("NORMAL");
					fastgltf::Accessor& normalAccessor = gltf.accessors[(normals)->second];
					fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, normalAccessor, [&](glm::vec3 v, size_t index)
					{
						vertices[initialVert + index].normal = v;
					});
				}
				// load UVs
				{
					auto uv = primitive.findAttribute("TEXCOORD_0");
					if (uv != primitive.attributes.end())
					{
						fastgltf::Accessor& uvAccessor = gltf.accessors[uv->second];
						fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, uvAccessor, [&](glm::vec2 v, size_t index)
						{
							vertices[initialVert + index].uv_x = v.x;
							vertices[initialVert + index].uv_y = v.y;
						});
					}
				}

				// load vertex colors
				{
					auto colors = primitive.findAttribute("COLOR_0");
					if (colors != primitive.attributes.end())
					{
						fastgltf::Accessor& colorAccessor = gltf.accessors[colors->second];
						fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, colorAccessor, [&](glm::vec4 v, size_t index)
						{
							vertices[initialVert + index].color = v;
						});
					}
				}

				if (primitive.materialIndex.has_value())
				{
					newSurface.material = materials[primitive.materialIndex.value()]; //Assign the material to the surface
				}
				else
				{
					newSurface.material = materials[0]; //Default material if no material is assigned
				}

				newMesh->surfaces.push_back(newSurface); //Add the surface to the mesh
			}

			newMesh->meshBuffers = engine->UploadMesh(indices, vertices); //Upload the meshs to the GPU
		}
	}

	//load nodes & meshes
	for (fastgltf::Node& node : gltf.nodes)
	{
		std::shared_ptr<RenderNode> newNode;

		if (node.meshIndex.has_value())
		{
			newNode = std::make_shared<MeshNode>(); //Create a new node with the mesh
			static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex]; //Assign the mesh to the node
		}
		else
		{
			newNode = std::make_shared<RenderNode>(); //Create a new node without a mesh
		}

		renderNodes.push_back(newNode);
		file.renderNodes[node.name.c_str()] = newNode; //Push the node into the map

		std::visit(fastgltf::visitor{
				[&](fastgltf::Node::TransformMatrix matrix)
				{
					memcpy(&newNode->localTransform, matrix.data(), sizeof(glm::mat4)); //Copy the matrix data to the node	
				},
				[&](fastgltf::Node::TRS transform)
				{
					glm::vec3 trans(transform.translation[0], transform.translation[1], transform.translation[2]);
					glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
					glm::vec3 scale(transform.scale[0], transform.scale[1], transform.scale[2]);

					glm::mat4 transMat = glm::translate(glm::mat4(1.f), trans);
					glm::mat4 rotMat = glm::toMat4(rot);
					glm::mat4 scaleMat = glm::scale(glm::mat4(1.f), scale);

					newNode->localTransform = transMat * rotMat * scaleMat; //Combine the matrices
				}
			}, node.transform);
	}
	

	// run loop to setup hiearchy
	for (int i = 0; i < gltf.nodes.size(); i++)
	{
		fastgltf::Node& node = gltf.nodes[i];
		std::shared_ptr<RenderNode>& sceneNode = renderNodes[i];
		
		for (auto& c : node.children)
		{
			sceneNode->children.push_back(renderNodes[c]); //Add the child node to the parent node
			renderNodes[c]->parent = sceneNode; //Set the parent of the child node
		}
	}

	// Find root nodes
	for (auto & node : renderNodes)
	{
		if (node->parent.lock() == nullptr) //If the node has no parent, it is a root node
		{
			file.topNodes.push_back(node);
			node->RefreshTransform(glm::mat4(1.f));
		}
	}

	//Debug
	fmt::println("Loaded images: fPath: {}, Vector: {}, BufferView: {}", fPath, Vector, BView);
	fmt::println("Loaded: Materials: {}, Meshes: {}, Samplers: {}", Materials, Meshes, Samplers);
	return loadedScene;
}

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
		constexpr bool OverrideColors = false;

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
