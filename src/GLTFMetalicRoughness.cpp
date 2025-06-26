#include "GLTFMetalicRoughness.h"
#include "vk_engine.h"

void GLTFMetallicRoughness::BuildPipelines(VulkanEngine* engine)
{
    VkShaderModule meshFragShader;
    if (!vkutil::LoadShaderModule("../../shaders/mesh.frag.spv", engine->_device, &meshFragShader))
    {
        fmt::println("Error when building mesh.frag.spv");
    }
    VkShaderModule meshVertexShader;
    if (!vkutil::LoadShaderModule("../../shaders/mesh.vert.spv", engine->_device, &meshVertexShader))
    {
        fmt::println("Error when building mesh.vert.spv");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    {
        VKDescriptors::DescriptorLayoutBuilder builder;
        materialLayout = builder
            .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            .AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            .Build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    
    VkDescriptorSetLayout layouts[] =
    {
        engine->_gpuSceneDataDescriptorLayout,
        materialLayout
    };

    VkPipelineLayoutCreateInfo meshLayout = vkinit::pipeline_layout_create_info();
    meshLayout.setLayoutCount = 2;
    meshLayout.pSetLayouts = layouts;
    meshLayout.pPushConstantRanges = &matrixRange;
    meshLayout.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &meshLayout, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    {
        vkutil::PipelineBuilder builder;
        builder = builder
            .SetShaders(meshVertexShader, meshFragShader)
            .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE)
            .SetMultisamplingNone()
            .EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
            .DisableBlending()

            .SetColorAttachmentFormat(engine->_drawImage.imageFormat)
            .SetDepthFormat(engine->_depthImage.imageFormat)
            .SetPipelineLayout(newLayout);
        opaquePipeline.pipeline = builder
            .BuildPipeline(engine->_device);

        transparentPipeline.pipeline = builder
            .EnableBlendingAdditive()
            .EnableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL)
            .BuildPipeline(engine->_device);
        
        vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
        vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);
    }
}

void GLTFMetallicRoughness::ClearResources(VkDevice device)
{
    DeletePipeline(device, opaquePipeline);
    DeletePipeline(device, transparentPipeline);
    vkDestroyPipelineLayout(device, opaquePipeline.layout, nullptr); //We share the same layout object
    vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
}

void GLTFMetallicRoughness::DeletePipeline(VkDevice device, MaterialPipeline pipeline)
{
    vkDestroyPipeline(device, pipeline.pipeline, nullptr);
}

MaterialInstance GLTFMetallicRoughness::WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, VKDescriptors::DescriptorAllocatorGrowable& descriptorAllocator)
{
    MaterialInstance matInstance;

    matInstance.passType = pass;

    switch (pass)
    {
    case MaterialPass::MainColor:
        matInstance.pipeline = &opaquePipeline;
        break;
    case MaterialPass::Transparent:
        matInstance.pipeline = &transparentPipeline;
        break;
    case MaterialPass::Other:
        fmt::println("Invalid Material on Object."); //At some point I want to add a UUID, and naming system for objects.
        break;
    default:
        break;
    }

    matInstance.materialSet = descriptorAllocator.Allocate(device, materialLayout);

    writer.Clear();
    writer
        .WriteBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .WriteImage(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        .WriteImage(2, resources.metalRoughImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        .UpdateSet(device, matInstance.materialSet);

    return matInstance;
}
