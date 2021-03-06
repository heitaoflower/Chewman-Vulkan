// VSE (Vulkan Simple Engine) Library
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under the MIT License
#pragma once
#include <string>
#include <vector>
#include "Engine.h"

namespace SVE
{

enum class TextureType : uint8_t
{
    ImageFile,
    ShadowMapDirect,
    ShadowMapPoint,
    Reflection,
    Refraction,
    ScreenQuad,
    ScreenQuadSecond, // after late pass (with particles)
    ScreenQuadDepth,
    LastEffect,
};

enum class TextureAddressMode : uint8_t
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
    MirrorClampToEdge
};

enum class TextureBorderColor : uint8_t
{
    TransparentBlack,
    SolidBlack,
    SolidWhite
};

enum class MaterialCullFace : uint8_t
{
    FrontFace,
    BackFace,
    None
};

enum class BlendFactor : uint8_t
{
    SrcAlpha,
    DstAlpha,
    OneMinusSrcAlpha,
    OneMinusDstAlpha,
    One,
    Zero
};

enum class MaterialQuality : uint8_t
{
    Low, // Loaded by low, med and high
    Medium, // loaded by med and high
    High // loaded by only high quality settings
};

struct TextureInfo
{
    TextureType textureType = TextureType::ImageFile;
    std::string textureSubtype;
    TextureAddressMode textureAddressMode = TextureAddressMode::Repeat;
    TextureBorderColor textureBorderColor = TextureBorderColor::TransparentBlack;
    std::string samplerName;
    std::string filename;
    uint32_t layers = 1;
    glm::ivec2 spritesheetSize;
    // TODO: Add possibility to enable/disable mipmap generation
};

struct MaterialSettings
{
    std::string name;

    std::string vertexShaderName;
    std::string fragmentShaderName;
    std::string geometryShaderName;

    std::vector<TextureInfo> textures;
    bool isCubemap = false;
    bool useDepthTest = true;
    bool useDepthWrite = true;
    bool useDepthBias = false;
    bool useMultisampling = true;
    bool useAlphaBlending = false;
    bool useMRT = false;
    bool useInstancing = false;
    bool ignoreShadow = true;
    uint32_t instanceMaxCount = 0;
    BlendFactor srcBlendFactor = BlendFactor::SrcAlpha;
    BlendFactor dstBlendFactor = BlendFactor::OneMinusSrcAlpha;
    MaterialCullFace cullFace = MaterialCullFace::FrontFace;
    CommandsType passType = CommandsType::MainPass; // used to select correct renderpass
    MaterialQuality loadQuality = MaterialQuality::Low; // load always
};

} // namespace SVE