// VSE (Vulkan Simple Engine) Library
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under CC BY 4.0#include "VulkanMaterial.h"
#include "VulkanMaterial.h"
#include "VulkanException.h"
#include "VulkanInstance.h"
#include "ShaderManager.h"
#include "ShaderInfo.h"
#include "Entity.h"
#include "Engine.h"
#include <fstream>
#include <algorithm>
//#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <cmath>

namespace SVE
{
SVE::VulkanMaterial::VulkanMaterial(MaterialSettings materialSettings)
    : _materialSettings(std::move(materialSettings))
    , _vulkanInstance(Engine::getInstance()->getVulkanInstance())
    , _device(_vulkanInstance->getLogicalDevice())
    , _vulkanUtils(_vulkanInstance->getVulkanUtils())
{
    const auto& shaderManager = Engine::getInstance()->getShaderManager();

    if (!_materialSettings.vertexShaderName.empty())
    {
        _vertexShader = shaderManager->getShader(_materialSettings.vertexShaderName)->getVulkanShaderInfo();
        _shaderList.push_back(_vertexShader);
    }
    if (!_materialSettings.fragmentShaderName.empty())
    {
        _fragmentShader = shaderManager->getShader(_materialSettings.fragmentShaderName)->getVulkanShaderInfo();
        _shaderList.push_back(_fragmentShader);
    }
    if (!_materialSettings.geometryShaderName.empty())
    {
        _geometryShader = shaderManager->getShader(_materialSettings.geometryShaderName)->getVulkanShaderInfo();
        _shaderList.push_back(_geometryShader);
    }

    createPipelineLayout();
    createPipeline();

    createTextureImages();
    createTextureImageView();
    createTextureSampler();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
}

VulkanMaterial::~VulkanMaterial()
{
    deleteDescriptorSets();
    deleteDescriptorPool();
    deleteUniformBuffers();

    deleteTextureSampler();
    deleteTextureImageView();
    deleteTextureImages();

    deletePipeline();
    deletePipelineLayout();
}

VkPipeline VulkanMaterial::getPipeline() const
{
    return _pipeline;
}

VkPipelineLayout VulkanMaterial::getPipelineLayout() const
{
    return _pipelineLayout;
}

std::vector<VkDescriptorSet> VulkanMaterial::getDescriptorSets(uint32_t materialIndex, size_t index) const
{
    std::vector<VkDescriptorSet> sets;
    sets.reserve(_shaderList.size());
    if (!_instanceData[materialIndex].vertexDescriptorSets.empty())
        sets.push_back(_instanceData[materialIndex].vertexDescriptorSets[index]);
    if (!_instanceData[materialIndex].fragmentDescriptorSets.empty())
        sets.push_back(_instanceData[materialIndex].fragmentDescriptorSets[index]);
    if (!_instanceData[materialIndex].geometryDescriptorSets.empty())
        sets.push_back(_instanceData[materialIndex].geometryDescriptorSets[index]);

    return sets;
}

uint32_t VulkanMaterial::getInstanceForEntity(Entity* entity)
{
    auto instanceIter = _entityInstanceMap.find(entity);
    if (instanceIter != _entityInstanceMap.end())
        return instanceIter->second;

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    _entityInstanceMap[entity] = _instanceData.size() - 1;
    return _instanceData.size() - 1;
}

void VulkanMaterial::setUniformData(uint32_t materialIndex, const UniformData& uniformData) const
{
    auto swapchainSize = _vulkanInstance->getSwapchainSize();
    auto imageIndex = _vulkanInstance->getCurrentImageIndex();
    for (auto i = 0u; i < _shaderList.size(); i++)
    {
        size_t uniformSize = _shaderList[i]->getShaderUniformsSize();
        if (uniformSize == 0)
            continue;
        const auto& shaderSettings = _shaderList[i]->getShaderSettings();
        void* data = nullptr;
        vkMapMemory(_device,  _instanceData[materialIndex].uniformBuffersMemory[swapchainSize * i + imageIndex], 0, uniformSize, 0, &data);
        char* mappedData = reinterpret_cast<char*>(data);
        for (auto r = 0u; r < shaderSettings.uniformList.size(); r++)
        {
            auto uniformBytes = getUniformDataByType(uniformData, shaderSettings.uniformList[r].uniformType);
            memcpy(mappedData, uniformBytes.data(), uniformBytes.size());
            mappedData += uniformBytes.size();
        }
        vkUnmapMemory(_device, _instanceData[materialIndex].uniformBuffersMemory[swapchainSize * i + imageIndex]);
    }
}

void VulkanMaterial::createPipeline()
{
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for (auto * shader : _shaderList)
    {
        shaderStages.push_back(shader->createShaderStage());
    }

    // Triangle data setup
    auto bindingDescriptions = _vertexShader->getBindingDescription();
    auto attributeDescriptions = _vertexShader->getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = bindingDescriptions.size();
    vertexInputStateCreateInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
    inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

    // Viewport
    auto extent = _vulkanInstance->getExtent();
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) extent.width;
    viewport.height = (float) extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportCreateInfo{};
    viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportCreateInfo.viewportCount = 1;
    viewportCreateInfo.pViewports = &viewport;
    viewportCreateInfo.scissorCount = 1; // should be the same as viewport count
    viewportCreateInfo.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo{};
    rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationCreateInfo.depthClampEnable = VK_FALSE; // clamp objects beyond near and far plane to the edges
    rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationCreateInfo.lineWidth = 1.0f;
    rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationCreateInfo.depthBiasEnable = VK_FALSE; // for altering depth (used in shadow mapping)

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampleCreateInfo{};
    multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleCreateInfo.rasterizationSamples = _vulkanInstance->getMSAASamples();
    multisampleCreateInfo.minSampleShading = 1.0f;
    multisampleCreateInfo.pSampleMask = nullptr;
    multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleCreateInfo.alphaToOneEnable = VK_FALSE;

    // Depth and stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo {};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

    // Blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blendingCreateInfo{};
    blendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendingCreateInfo.logicOpEnable = VK_FALSE;
    blendingCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    blendingCreateInfo.attachmentCount = 1;
    blendingCreateInfo.pAttachments = &colorBlendAttachment;
    //blendingCreateInfo.blendConstants[0] = 0.0f;

    // Finally create pipeline
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
    pipelineCreateInfo.pColorBlendState = &blendingCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.layout = _pipelineLayout;
    pipelineCreateInfo.renderPass = _vulkanInstance->getRenderPass();
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // no deriving from other pipeline
    pipelineCreateInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_pipeline) !=
        VK_SUCCESS)
    {
        throw VulkanException("Can't create Vulkan Graphics Pipeline");
    }

    for (auto * shader : _shaderList)
    {
        shader->freeShaderModule();
    }
}


void VulkanMaterial::deletePipeline()
{
    vkDestroyPipeline(_device, _pipeline, nullptr);
}


void VulkanMaterial::createPipelineLayout()
{
    std::vector<VkDescriptorSetLayout> descriptorLayouts;

    for (auto* shader : _shaderList)
    {
        descriptorLayouts.push_back(shader->getDescriptorSetLayout());
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = descriptorLayouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = descriptorLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(
            _device,
            &pipelineLayoutCreateInfo,
            nullptr,
            &_pipelineLayout) != VK_SUCCESS)
    {
        throw VulkanException("Can't create Vulkan pipeline layout");
    }
}

void VulkanMaterial::deletePipelineLayout()
{
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
}

void VulkanMaterial::createTextureImages()
{
    size_t imageCount = _materialSettings.textures.size();
    _mipLevels.resize(imageCount);
    _textureImages.resize(imageCount);
    _textureImageMemoryList.resize(imageCount);
    _textureNames.resize(imageCount);
    for (auto i = 0u; i < imageCount; i++)
    {
        // Load image pixel data
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(_materialSettings.textures[i].filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        _mipLevels[i] = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

        if (!pixels)
        {
            throw VulkanException("Can't load texture " + _materialSettings.textures[i].filename);
        }

        // Create temporary buffer to hold pixel data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        _vulkanUtils.createBuffer(imageSize,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  stagingBuffer,
                                  stagingBufferMemory);

        // Copy pixel data into temporary buffer
        void* data;
        vkMapMemory(_device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(_device, stagingBufferMemory);

        // Free pixel data
        stbi_image_free(pixels);

        // Create texture image which will be used in shaders
        _vulkanUtils.createImage(texWidth,
                                 texHeight,
                                 _mipLevels[i],
                                 VK_SAMPLE_COUNT_1_BIT,
                                 VK_FORMAT_R8G8B8A8_UNORM,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                 VK_IMAGE_USAGE_SAMPLED_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 _textureImages[i],
                                 _textureImageMemoryList[i]);

        // Transition layout of the image to be optimal as a transfer destination
        _vulkanUtils.transitionImageLayout(
                _textureImages[i],
                VK_FORMAT_R8G8B8A8_UNORM,
                {VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT},
                {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT},
                _mipLevels[i]);
        // Copy image data from buffer to image
        _vulkanUtils.copyBufferToImage(stagingBuffer,
                                       _textureImages[i],
                                       static_cast<uint32_t>(texWidth),
                                       static_cast<uint32_t>(texHeight));
        // Transition image layout from transfer destination to optimal for shader usage
        /*transitionImageLayout(_textureImage,
                              VK_FORMAT_R8G8B8A8_UNORM,
                              {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT},
                              {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
                              _mipLevels);*/
        _vulkanUtils.generateMipmaps(_textureImages[i], VK_FORMAT_R8G8B8A8_UNORM, texWidth, texHeight, _mipLevels[i]);

        // Free temporary buffer
        vkDestroyBuffer(_device, stagingBuffer, nullptr);
        vkFreeMemory(_device, stagingBufferMemory, nullptr);

        _textureNames[i] = _materialSettings.textures[i].samplerName;
    }
}

void VulkanMaterial::deleteTextureImages()
{
    for (auto i = 0u; i < _textureImages.size(); i++)
    {
        vkDestroyImage(_device, _textureImages[i], nullptr);
        vkFreeMemory(_device, _textureImageMemoryList[i], nullptr);
    }
}

void VulkanMaterial::createTextureImageView()
{
    _textureImageViews.resize(_textureImages.size());
    for (auto i = 0; i < _textureImages.size(); i++)
    {
        _textureImageViews[i] = _vulkanUtils.createImageView(_textureImages[i], VK_FORMAT_R8G8B8A8_UNORM, _mipLevels[i]);
    }
}

void VulkanMaterial::deleteTextureImageView()
{
    for (auto i = 0; i < _textureImageViews.size(); i++)
    {
        vkDestroyImageView(_device, _textureImageViews[i], nullptr);
    }
}

void VulkanMaterial::createTextureSampler()
{
    _textureSamplers.resize(_textureImages.size());

    for (auto i = 0; i < _textureImages.size(); i++)
    {
        VkSamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
        samplerCreateInfo.maxAnisotropy = 16;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
        samplerCreateInfo.compareEnable = VK_FALSE;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.minLod = 0;
        samplerCreateInfo.maxLod = _mipLevels[i];
        samplerCreateInfo.mipLodBias = 0;

        if (vkCreateSampler(_device, &samplerCreateInfo, nullptr, &_textureSamplers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Can't create Vulkan texture sampler");
        }
    }
}

void VulkanMaterial::deleteTextureSampler()
{
    for (auto i = 0; i < _textureImages.size(); i++)
    {
        vkDestroySampler(_device, _textureSamplers[i], nullptr);
    }
}

void VulkanMaterial::createUniformBuffers()
{
    auto swapchainSize = _vulkanInstance->getSwapchainSize();
    size_t memoryBufferOffset = 0;
    PerInstanceData data {};

    auto createUniformBuffers = [this, &data, &swapchainSize, &memoryBufferOffset](std::vector<VkBuffer>& buffers,
                                                                                   const VulkanShaderInfo* shaderInfo)
    {
        if (!shaderInfo)
            return;

        VkDeviceSize bufferSize = shaderInfo->getShaderUniformsSize();
        if (bufferSize == 0)
            return;

        buffers.resize(swapchainSize);
        data.uniformBuffersMemory.resize(data.uniformBuffersMemory.size() + swapchainSize);

        for (auto i = 0u; i < swapchainSize; i++)
        {
            _vulkanUtils.createBuffer(
                    bufferSize,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    buffers[i],
                    data.uniformBuffersMemory[memoryBufferOffset + i]);
        }
        memoryBufferOffset += swapchainSize;
    };

    createUniformBuffers(data.vertexUniformBuffers, _vertexShader);
    createUniformBuffers(data.fragmentUniformBuffer, _fragmentShader);
    createUniformBuffers(data.geometryUniformBuffer, _geometryShader);

    _instanceData.push_back(data);
}

void VulkanMaterial::deleteUniformBuffers()
{
    for (auto& instance : _instanceData)
    {
        for (auto buffer : instance.vertexUniformBuffers)
        {
            vkDestroyBuffer(_device, buffer, nullptr);
        }
        for (auto buffer : instance.fragmentUniformBuffer)
        {
            vkDestroyBuffer(_device, buffer, nullptr);
        }
        for (auto buffer : instance.geometryUniformBuffer)
        {
            vkDestroyBuffer(_device, buffer, nullptr);
        }
        for (auto memory : instance.uniformBuffersMemory)
        {
            vkFreeMemory(_device, memory, nullptr);
        }
    }
}


void VulkanMaterial::createDescriptorPool()
{
    auto swapchainSize = _vulkanInstance->getSwapchainSize();

    auto& instance = _instanceData.back();

    std::vector<VkDescriptorPoolSize> poolSizes(2);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = instance.vertexUniformBuffers.size() + instance.fragmentUniformBuffer.size() + instance.geometryUniformBuffer.size();
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = swapchainSize * _materialSettings.textures.size();

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = _shaderList.size() * swapchainSize;

    if (vkCreateDescriptorPool(_device, &poolInfo, nullptr, &instance.descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Can't create Vulkan descriptor pool");
    }
}

void VulkanMaterial::deleteDescriptorPool()
{
    for (auto &instance : _instanceData)
    {
        vkDestroyDescriptorPool(_device, instance.descriptorPool, nullptr);
    }
}

void VulkanMaterial::createDescriptorSets()
{
    auto swapchainSize = _vulkanInstance->getSwapchainSize();
    auto makeDescriptorSet = [this, swapchainSize](const std::vector<VkBuffer>& shaderBuffers,
                                                   const VulkanShaderInfo* shaderInfo,
                                                   std::vector<VkDescriptorSet>& descriptorSets)
    {
        if (!shaderInfo)
            return;
        if (shaderInfo->getShaderSettings().samplerNamesList.empty() && shaderBuffers.empty())
            return;

        std::vector<VkDescriptorSetLayout> layouts(swapchainSize, shaderInfo->getDescriptorSetLayout());

        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _instanceData.back().descriptorPool;
        allocInfo.descriptorSetCount = swapchainSize;
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(swapchainSize);
        auto result = vkAllocateDescriptorSets(_device, &allocInfo, descriptorSets.data());
        if (result != VK_SUCCESS)
        {
            throw VulkanException("Can't allocate Vulkan descriptor sets");
        }

        auto uniformSize = shaderInfo->getShaderUniformsSize();

        for (auto i = 0u; i < swapchainSize; i++)
        {
            VkDescriptorBufferInfo bufferInfo {};
            if (!shaderBuffers.empty())
            {
                bufferInfo.buffer = shaderBuffers[i];
                bufferInfo.offset = 0;
                bufferInfo.range = uniformSize;
            }

            std::vector<VkDescriptorImageInfo> imageInfoList;
            for (auto r = 0u; r < shaderInfo->getShaderSettings().samplerNamesList.size(); r++)
            {
                auto iter = std::find(_textureNames.cbegin(),
                                      _textureNames.cend(),
                                      shaderInfo->getShaderSettings().samplerNamesList[r]);
                if (iter == _textureNames.end())
                {
                    throw VulkanException("Incorrect sampler name in material configuration");
                }
                auto index = std::distance(_textureNames.cbegin(), iter);

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = _textureImageViews[index];
                imageInfo.sampler = _textureSamplers[index];

                imageInfoList.push_back(imageInfo);
            }


            std::vector<VkWriteDescriptorSet> descriptorWrites;
            auto bindingIndex = 0u;
            // Add texture samplers
            if (!imageInfoList.empty())
            {
                VkWriteDescriptorSet imagesBuffer {};
                imagesBuffer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                imagesBuffer.dstSet = descriptorSets[i];
                imagesBuffer.dstBinding = bindingIndex;
                imagesBuffer.dstArrayElement = 0;
                imagesBuffer.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                imagesBuffer.descriptorCount = imageInfoList.size();
                imagesBuffer.pImageInfo = imageInfoList.data();
                descriptorWrites.push_back(imagesBuffer);
                bindingIndex++;
            }
            // Add uniforms
            if (!shaderBuffers.empty())
            {
                VkWriteDescriptorSet uniformsBuffer {};
                uniformsBuffer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                uniformsBuffer.dstSet = descriptorSets[i];
                uniformsBuffer.dstBinding = bindingIndex;
                uniformsBuffer.dstArrayElement = 0;
                uniformsBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                uniformsBuffer.descriptorCount = 1;
                uniformsBuffer.pBufferInfo = &bufferInfo;
                descriptorWrites.push_back(uniformsBuffer);
            }

            vkUpdateDescriptorSets(_device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
        }
    };

    makeDescriptorSet(_instanceData.back().vertexUniformBuffers, _vertexShader, _instanceData.back().vertexDescriptorSets);
    makeDescriptorSet(_instanceData.back().fragmentUniformBuffer, _fragmentShader, _instanceData.back().fragmentDescriptorSets);
    makeDescriptorSet(_instanceData.back().geometryUniformBuffer, _geometryShader, _instanceData.back().geometryDescriptorSets);
}

void VulkanMaterial::deleteDescriptorSets()
{

}

std::vector<char> VulkanMaterial::getUniformDataByType(const UniformData& data, UniformType type) const
{
    const auto& sizeMap = getUniformSizeMap();
    switch (type)
    {
        case UniformType::ModelMatrix:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.model);
            return std::vector<char>(byteData, byteData + sizeof(data.model));
        }
        case UniformType::ViewMatrix:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.view);
            return std::vector<char>(byteData, byteData + sizeof(data.view));
        }
        case UniformType::ProjectionMatrix:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.projection);
            return std::vector<char>(byteData, byteData + sizeof(data.projection));
        }
        case UniformType::ModelViewProjectionMatrix:
        {
            glm::mat4 mvp = data.projection * data.view * data.model;
            const char* byteData = reinterpret_cast<const char*>(&mvp);
            return std::vector<char>(byteData, byteData + sizeof(mvp));
        }
        case UniformType::CameraPosition:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.cameraPos);
            return std::vector<char>(byteData, byteData + sizeof(data.cameraPos));
        }
        case UniformType::LightPosition:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.lightPos);
            return std::vector<char>(byteData, byteData + sizeMap.at(type));
        }
        case UniformType::LightColor:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.lightSettings.lightColor);
            return std::vector<char>(byteData, byteData + sizeMap.at(type));
        }
        case UniformType::LightAmbient:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.lightSettings.ambientStrength);
            return std::vector<char>(byteData, byteData + sizeof(data.lightSettings.ambientStrength));
        }
        case UniformType::LightDiffuse:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.lightSettings.diffuseStrength);
            return std::vector<char>(byteData, byteData + sizeof(data.lightSettings.diffuseStrength));
        }
        case UniformType::LightSpecular:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.lightSettings.specularStrength);
            return std::vector<char>(byteData, byteData + sizeof(data.lightSettings.specularStrength));
        }
        case UniformType::LightShininess:
        {
            const char* byteData = reinterpret_cast<const char*>(&data.lightSettings.shininess);
            return std::vector<char>(byteData, byteData + sizeof(data.lightSettings.shininess));
        }
        case UniformType::BoneMatrices:
        {
            const char* byteData = reinterpret_cast<const char*>(data.bones.data());
            return std::vector<char>(byteData, byteData + sizeMap.at(type) * data.bones.size());
        }
    }

    throw VulkanException("Unsupported uniform type");
}

} // namespace SVE