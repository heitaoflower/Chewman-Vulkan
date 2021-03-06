// VSE (Vulkan Simple Engine) Library
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under the MIT License
#include "ResourceManager.h"
#include "VulkanException.h"
#include "MaterialSettings.h"
#include "EngineSettings.h"
#include "ShaderSettings.h"
#include "MeshSettings.h"
#include "LightSettings.h"
#include "ParticleSystemManager.h"
#include "FontManager.h"

#include "Engine.h"
#include "ShaderManager.h"
#include "SceneManager.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "Libs.h"

#include <utf8.h>
#include <map>
#include <rapidjson/document.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#define setOptional(expr)                \
    try { expr; }                        \
    catch (const RapidJsonException&) { }

namespace SVE
{
namespace
{
namespace rj = rapidjson;

template<size_t vectorSize = 3, typename resultType = float, typename ObjectType>
glm::vec<vectorSize, resultType, glm::highp> loadVector(ObjectType &object, const std::string &name)
{
    auto vecArray = object[name.c_str()].GetArray();

    glm::vec<vectorSize, resultType, glm::highp> v;
    for (auto i = 0u; i < vectorSize; i++)
    {
        v[i] = vecArray[i].GetFloat();
    }

    return v;
}

EngineSettings loadEngine(const std::string& data)
{
    static const std::map<std::string, EngineSettings::PresentMode> presentModeMap{
            {"FIFO",          EngineSettings::PresentMode::FIFO},
            {"Mailbox",       EngineSettings::PresentMode::Mailbox},
            {"Immediate",     EngineSettings::PresentMode::Immediate},
            {"BestAvailable", EngineSettings::PresentMode::BestAvailable}
    };

    rj::Document document;
    document.Parse(data.c_str());

    EngineSettings engineSettings {};
    setOptional(engineSettings.useValidation = document["useValidation"].GetBool());
    setOptional(engineSettings.presentMode = presentModeMap.at(document["presentMode"].GetString()));
    setOptional(engineSettings.gpuIndex =
            document["gpuIndex"].IsString()
                ? (document["gpuIndex"].GetString() == std::string("best")
                        ? EngineSettings::BEST_GPU_AVAILABLE
                        : throw VulkanException("Incorrect gpu index"))
                : document["gpuIndex"].GetInt());
    setOptional(engineSettings.MSAALevel =
            document["MSAALevel"].IsString()
                ? (document["MSAALevel"].GetString() == std::string("best")
                       ? EngineSettings::BEST_MSAA_AVAILABLE
                       : throw VulkanException("Incorrect MSAA level"))
                : document["MSAALevel"].GetInt());
    setOptional(engineSettings.applicationName = document["applicationName"].GetString());
    setOptional(engineSettings.initShadows = document["initShadows"].GetBool());
    setOptional(engineSettings.initWater = document["initWater"].GetBool());
    setOptional(engineSettings.useScreenQuad = document["useScreenQuad"].GetBool());
    setOptional(engineSettings.useCascadeShadowMap = document["useCascadeShadowMap"].GetBool());
    setOptional(engineSettings.particlesEnabled = document["particlesEnabled"].GetBool());

    return engineSettings;
}

std::vector<UniformInfo> getUniformInfoList(rj::Document& document)
{
    static const std::map<std::string, UniformType> uniformMap{
            {"ModelMatrix",                     UniformType::ModelMatrix},
            {"ViewMatrix",                      UniformType::ViewMatrix},
            {"ProjectionMatrix",                UniformType::ProjectionMatrix},
            {"InverseModelMatrix",              UniformType::InverseModelMatrix},
            {"ModelViewProjectionMatrix",       UniformType::ModelViewProjectionMatrix},
            {"ViewProjectionMatrix",            UniformType::ViewProjectionMatrix},
            {"ViewProjectionMatrixList",        UniformType::ViewProjectionMatrixList},
            {"ViewProjectionMatrixSize",        UniformType::ViewProjectionMatrixSize},
            {"CameraPosition",                  UniformType::CameraPosition},
            {"MaterialInfo",                    UniformType::MaterialInfo},
            {"LightInfo",                       UniformType::LightInfo},
            {"LightDirectional",                UniformType::LightDirectional},
            {"LightPoint",                      UniformType::LightPoint},
            {"LightPointSimple",                UniformType::LightPointSimple},
            {"LightLine",                       UniformType::LightLine},
            {"LightSpot",                       UniformType::LightSpot},
            {"LightPointViewProjectionList",    UniformType::LightPointViewProjectionList},
            {"LightDirectViewProjectionList",   UniformType::LightDirectViewProjectionList},
            {"LightDirectViewProjection",       UniformType::LightDirectViewProjection},
            {"BoneMatrices",                    UniformType::BoneMatrices},
            {"ClipPlane",                       UniformType::ClipPlane},
            {"ParticleEmitter",                 UniformType::ParticleEmitter},
            {"ParticleAffector",                UniformType::ParticleAffector},
            {"ParticleCount",                   UniformType::ParticleCount},
            {"SpritesheetSize",                 UniformType::SpritesheetSize},
            {"ImageSize",                       UniformType::ImageSize},
            {"TextInfo",                        UniformType::TextInfo},
            {"GlyphInfoList",                   UniformType::GlyphInfoList},
            {"TextSymbolList",                  UniformType::TextSymbolList},
            {"OverlayInfo",                     UniformType::OverlayInfo},
            {"CustomFloat",                     UniformType::CustomFloat},
            {"CustomVec4",                      UniformType::CustomVec4},
            {"CustomMat4",                      UniformType::CustomMat4},
            {"Time",                            UniformType::Time},
            {"DeltaTime",                       UniformType::DeltaTime},
    };

    std::vector<UniformInfo> uniformList;
    auto list = document["uniformList"].GetArray();
    for (auto& item : list)
    {
        UniformInfo uniformInfo {};
        uniformInfo.uniformType = uniformMap.at(item["uniformType"].GetString());
        setOptional(uniformInfo.uniformIndex = item["uniformIndex"].GetInt());
        uniformList.push_back(std::move(uniformInfo));
    }

    return uniformList;
}

std::vector<BufferType> getBufferTypeList(rj::Document& document)
{
    static const std::map<std::string, BufferType> bufferMap{
            {"AtomicCounter",     BufferType::AtomicCounter },
            {"ModelMatrixList",   BufferType::ModelMatrixList },
            {"TextSymbolList",    BufferType::TextSymbolList },
    };

    std::vector<BufferType> bufferList;
    auto list = document["bufferList"].GetArray();
    for (auto& item : list)
    {
        auto bufferType = bufferMap.at(item.GetString());
        bufferList.push_back(bufferType);
    }

    return bufferList;
}

VertexInfo getVertexInfo(rj::Document& document)
{
    static const std::map<std::string, VertexInfo::VertexDataType> vertexDataTypeMap{
            {"Position",    VertexInfo::VertexDataType::Position},
            {"Color",       VertexInfo::VertexDataType::Color},
            {"TexCoord",    VertexInfo::VertexDataType::TexCoord},
            {"Normal",      VertexInfo::VertexDataType::Normal},
            {"Binormal",    VertexInfo::VertexDataType::Binormal},
            {"Tangent",     VertexInfo::VertexDataType::Tangent},
            {"BoneWeights", VertexInfo::VertexDataType::BoneWeights},
            {"BoneIds",     VertexInfo::VertexDataType::BoneIds},
            {"Custom",      VertexInfo::VertexDataType::Custom},
    };

    VertexInfo info {};

    auto vertexInfo = document["vertexInfo"].GetObject();
    auto vertexDataFlags = vertexInfo["vertexDataFlags"].GetArray();

    info.vertexDataFlags = 0;
    for (auto& item : vertexDataFlags)
    {
        info.vertexDataFlags |= vertexDataTypeMap.at(item.GetString());
    }
    setOptional(info.positionSize = static_cast<uint8_t>(vertexInfo["positionSize"].GetUint()));
    setOptional(info.colorSize = static_cast<uint8_t>(vertexInfo["colorSize"].GetUint()));
    setOptional(info.customCount = static_cast<uint8_t>(vertexInfo["customCount"].GetUint()));
    setOptional(info.separateBinding = static_cast<uint8_t>(vertexInfo["separateBinding"].GetBool()));

    return info;
}

std::vector<std::string> getStringList(rj::Document& document, const std::string& listName)
{
    auto list = document[listName.c_str()].GetArray();

    std::vector<std::string> stringList;
    for (auto& item : list)
    {
        stringList.emplace_back(item.GetString());
    }

    return stringList;
}

ShaderSettings loadShader(FSEntityPtr directory, const std::string& data)
{
    static const std::map<std::string, ShaderType> shaderTypeMap{
            {"VertexShader",   ShaderType::VertexShader},
            {"FragmentShader", ShaderType::FragmentShader},
            {"GeometryShader", ShaderType::GeometryShader},
            {"ComputeShader",  ShaderType::ComputeShader},
    };

    rj::Document document;
    document.Parse(data.c_str());

    ShaderSettings shaderSettings {};
    shaderSettings.name = document["name"].GetString();
    setOptional(shaderSettings.maxBonesSize = document["maxBonesSize"].GetUint());
    setOptional(shaderSettings.maxLightSize = document["maxLightSize"].GetUint());
    setOptional(shaderSettings.maxCascadeLightSize = document["maxCascadeLightSize"].GetUint());
    setOptional(shaderSettings.maxShadowPointLightSize = document["maxShadowPointLightSize"].GetUint());
    setOptional(shaderSettings.maxLineLightSize = document["maxLineLightSize"].GetUint());
    setOptional(shaderSettings.maxViewProjectionMatrices = document["maxViewProjectionMatrices"].GetUint());
    setOptional(shaderSettings.uniformList = getUniformInfoList(document));
    setOptional(shaderSettings.bufferList = getBufferTypeList(document));
    setOptional(shaderSettings.vertexInfo = getVertexInfo(document));
    setOptional(shaderSettings.maxGlyphCount = document["maxGlyphCount"].GetUint());
    setOptional(shaderSettings.samplerNamesList = getStringList(document, "samplerNamesList"));
    shaderSettings.filename = directory->resolveFilePath(document["filename"].GetString());
    shaderSettings.shaderType = shaderTypeMap.at(document["shaderType"].GetString());
    setOptional(shaderSettings.entryPoint = document["entryPoint"].GetString());

    return shaderSettings;
}

std::vector<TextureInfo> getTextureInfos(FSEntityPtr directory, rj::Document& document)
{
    static const std::map<std::string, TextureType> textureTypeMap{
            {"ImageFile",       TextureType::ImageFile},
            {"ShadowMapDirect", TextureType::ShadowMapDirect},
            {"ShadowMapPoint",  TextureType::ShadowMapPoint},
            {"Reflection",      TextureType::Reflection},
            {"Refraction",      TextureType::Refraction},
            {"ScreenQuad",      TextureType::ScreenQuad},
            {"ScreenQuadSecond",TextureType::ScreenQuadSecond},
            {"ScreenQuadDepth", TextureType::ScreenQuadDepth},
            {"LastEffect",      TextureType::LastEffect},
    };

    static const std::map<std::string, TextureAddressMode> addressModeMap {
            { "Repeat",             TextureAddressMode::Repeat },
            { "MirroredRepeat",     TextureAddressMode::MirroredRepeat },
            { "ClampToEdge",        TextureAddressMode::ClampToEdge },
            { "ClampToBorder",      TextureAddressMode::ClampToBorder },
            { "MirrorClampToEdge",  TextureAddressMode::MirrorClampToEdge },
    };

    static const std::map<std::string, TextureBorderColor> borderColorMap {
            { "TransparentBlack",   TextureBorderColor::TransparentBlack },
            { "SolidBlack",         TextureBorderColor::SolidBlack },
            { "SolidWhite",         TextureBorderColor::SolidWhite },
    };

    auto list = document["textures"].GetArray();
    std::vector<TextureInfo> textureInfosList;

    for (auto& item : list)
    {
        TextureInfo textureInfo {};

        setOptional(textureInfo.textureType = textureTypeMap.at(item["textureType"].GetString()));
        setOptional(textureInfo.textureSubtype = item["textureSubtype"].GetString());
        setOptional(textureInfo.textureAddressMode = addressModeMap.at(item["textureAddressMode"].GetString()));
        setOptional(textureInfo.textureBorderColor = borderColorMap.at(item["textureBorderColor"].GetString()));
        setOptional(textureInfo.layers = item["layers"].GetUint());
        setOptional((textureInfo.spritesheetSize = loadVector<2,int>(item, "spritesheetSize")));

        if (textureInfo.textureType == TextureType::ImageFile)
        {
            textureInfo.filename = directory->resolveFilePath(item["filename"].GetString());
        }
        textureInfo.samplerName = item["samplerName"].GetString();

        textureInfosList.push_back(std::move(textureInfo));
    }

    return textureInfosList;
}

ParticleSystemSettings loadParticleSystem(FSEntityPtr directory, const std::string& data)
{
    rj::Document document;
    document.Parse(data.c_str());

    ParticleSystemSettings particleSettings {};
    particleSettings.name = document["name"].GetString();
    particleSettings.materialName = document["materialName"].GetString();
    particleSettings.computeShaderName = document["computeShaderName"].GetString();
    particleSettings.quota = document["quota"].GetUint();
    particleSettings.sort = document["sort"].GetBool();

    ParticleEmitter emitter {};
    auto emitterObject = document["particleEmitter"].GetObject();

    glm::vec3 direction = loadVector(emitterObject, "direction");
    emitter.toDirection = glm::toMat4(glm::rotation(glm::vec3(0,0,1), direction));
    emitter.angle = emitterObject["angle"].GetFloat();
    emitter.originRadius = emitterObject["originRadius"].GetFloat();

    emitter.emissionRate = emitterObject["emissionRate"].GetFloat();
    emitter.minLife = emitterObject["minLife"].GetFloat();
    emitter.maxLife = emitterObject["maxLife"].GetFloat();
    emitter.minSpeed = emitterObject["minSpeed"].GetFloat();
    emitter.maxSpeed = emitterObject["maxSpeed"].GetFloat();
    emitter.minSize = emitterObject["minSize"].GetFloat();
    emitter.maxSize = emitterObject["maxSize"].GetFloat();
    setOptional(emitter.sizeScale = emitterObject["sizeScale"].GetFloat());
    emitter.minRotate = emitterObject["minRotate"].GetFloat();
    emitter.maxRotate = emitterObject["maxRotate"].GetFloat();
    emitter.colorRangeStart = loadVector<4>(emitterObject, "colorRangeStart");
    emitter.colorRangeEnd = loadVector<4>(emitterObject, "colorRangeEnd");

    ParticleAffector affector {};
    auto affectorObject = document["particleAffector"].GetObject();
    affector.minAcceleration = affectorObject["minAcceleration"].GetFloat();
    affector.maxAcceleration = affectorObject["maxAcceleration"].GetFloat();
    affector.minRotateSpeed = affectorObject["minRotateSpeed"].GetFloat();
    affector.maxRotateSpeed = affectorObject["maxRotateSpeed"].GetFloat();
    affector.minScaleSpeed = affectorObject["minScaleSpeed"].GetFloat();
    affector.maxScaleSpeed = affectorObject["maxScaleSpeed"].GetFloat();
    affector.colorChanger = loadVector<4>(affectorObject, "colorChanger");

    particleSettings.particleEmitter = std::move(emitter);
    particleSettings.particleAffector = std::move(affector);

    return particleSettings;
}

Font loadFont(FSEntityPtr directory, const std::string& data)
{
    rj::Document document;
    document.Parse(data.c_str());

    Font font {};
    font.fontName = document["name"].GetString();
    font.materialName = document["material"].GetString();
    font.width = document["width"].GetUint();
    font.height = document["height"].GetUint();
    font.size = document["size"].GetUint();
    font.maxHeight = 0;

    auto characters = document["characters"].GetObject();
    uint32_t symbolIndex = 0;
    for (auto& character : characters)
    {
        auto characterInfo = character.value.GetObject();
        GlyphInfo glyphInfo {};
        glyphInfo.x = characterInfo["x"].GetUint();
        glyphInfo.y = characterInfo["y"].GetUint();
        glyphInfo.width = characterInfo["width"].GetUint();
        glyphInfo.height = characterInfo["height"].GetUint();
        glyphInfo.originX = characterInfo["originX"].GetInt();
        glyphInfo.originY = characterInfo["originY"].GetInt();
        glyphInfo.advance = characterInfo["advance"].GetUint();
        const char* charName = character.name.GetString();
        uint32_t characterCode = utf8::next(charName, charName + character.name.GetStringLength());
        font.symbolToInfoPos[characterCode] = symbolIndex;
        font.symbols[symbolIndex] = glyphInfo;
        font.maxHeight = std::max(font.maxHeight, glyphInfo.originY);
        ++symbolIndex;
    }

    for (auto i = 0; i < symbolIndex; ++i)
    {
        font.maxGlyphHeight = std::max(font.maxGlyphHeight, font.symbols[i].height + font.maxHeight - font.symbols[i].originY);
    }


    return font;
}

MaterialSettings loadMaterial(FSEntityPtr directory, const std::string& data)
{
    static const std::map<std::string, MaterialCullFace> cullFaceMap {
            { "BackFace",   MaterialCullFace::BackFace },
            { "FrontFace",  MaterialCullFace::FrontFace },
            { "None",       MaterialCullFace::None },
    };

    static const std::map<std::string, CommandsType> passTypeMap {
            { "MainPass",               CommandsType::MainPass },
            { "ScreenQuadPass",         CommandsType::ScreenQuadPass },
            { "ScreenQuadMRTPass",      CommandsType::ScreenQuadMRTPass },
            { "RefractionPass",         CommandsType::RefractionPass },
            { "ReflectionPass",         CommandsType::ReflectionPass },
            { "ShadowPassDirectLight",  CommandsType::ShadowPassDirectLight },
            { "ShadowPassPointLights",  CommandsType::ShadowPassPointLights },
    };

    static const std::map<std::string, BlendFactor> blendFactor {
            {"SrcAlpha",            BlendFactor::SrcAlpha },
            {"DstAlpha",            BlendFactor::DstAlpha },
            {"OneMinusSrcAlpha",    BlendFactor::OneMinusSrcAlpha },
            {"OneMinusDstAlpha",    BlendFactor::OneMinusDstAlpha },
            {"One",                 BlendFactor::One },
            {"Zero",                BlendFactor::Zero }
    };

    static const std::map<std::string, MaterialQuality> materialQuality {
            {"Low",            MaterialQuality::Low },
            {"High",           MaterialQuality::High },
            {"Medium",         MaterialQuality::Medium }
    };

    rj::Document document;
    document.Parse(data.c_str());

    MaterialSettings materialSettings {};
    materialSettings.name = document["name"].GetString();
    setOptional(materialSettings.cullFace = cullFaceMap.at(document["cullFace"].GetString()));
    setOptional(materialSettings.useDepthTest = document["useDepthTest"].GetBool());
    setOptional(materialSettings.useDepthWrite = document["useDepthWrite"].GetBool());
    setOptional(materialSettings.useDepthBias = document["useDepthBias"].GetBool());
    setOptional(materialSettings.useMultisampling = document["useMultisampling"].GetBool());
    setOptional(materialSettings.useAlphaBlending = document["useAlphaBlending"].GetBool());
    setOptional(materialSettings.useMRT = document["useMRT"].GetBool());
    setOptional(materialSettings.useInstancing = document["useInstancing"].GetBool());
    setOptional(materialSettings.ignoreShadow = document["ignoreShadow"].GetBool());
    setOptional(materialSettings.instanceMaxCount = document["instanceMaxCount"].GetUint());
    setOptional(materialSettings.srcBlendFactor = blendFactor.at(document["srcBlendFactor"].GetString()));
    setOptional(materialSettings.dstBlendFactor = blendFactor.at(document["dstBlendFactor"].GetString()));
    setOptional(materialSettings.isCubemap = document["isCubemap"].GetBool());
    setOptional(materialSettings.passType = passTypeMap.at(document["passType"].GetString()));
    setOptional(materialSettings.fragmentShaderName = document["fragmentShaderName"].GetString());
    setOptional(materialSettings.geometryShaderName = document["geometryShaderName"].GetString());
    setOptional(materialSettings.vertexShaderName = document["vertexShaderName"].GetString());
    setOptional(materialSettings.textures = getTextureInfos(directory, document));
    setOptional(materialSettings.loadQuality = materialQuality.at(document["loadQuality"].GetString()));

    return materialSettings;
}

MeshLoadSettings loadMesh(FSEntityPtr directory, const std::string& data)
{
    rj::Document document;
    document.Parse(data.c_str());

    MeshLoadSettings meshLoadSettings {};

    meshLoadSettings.filename = directory->resolveFilePath(document["filename"].GetString());
    meshLoadSettings.name = document["name"].GetString();
    setOptional(meshLoadSettings.switchYZ = document["switchYZ"].GetBool());
    setOptional(meshLoadSettings.scale = loadVector<3>(document, "scale"));
    setOptional(meshLoadSettings.animationSpeed = document["animationSpeed"].GetFloat());

    return meshLoadSettings;
}

LightSettings loadLight(const std::string& data)
{
    static const std::map<std::string, LightType> lightTypeMap{
            {"ShadowPointLight",    LightType::ShadowPointLight},
            {"PointLight",          LightType::PointLight},
            {"RectLight",           LightType::RectLight},
            {"SpotLight",           LightType::SpotLight},
            {"SunLight",            LightType::SunLight},
            {"LineLight",           LightType::LineLight},
    };

    rj::Document document;
    document.Parse(data.c_str());

    LightSettings lightSettings {};

    lightSettings.lightType =  lightTypeMap.at(document["lightType"].GetString());
    lightSettings.lightColor = loadVector(document, "lightColor");
    setOptional(lightSettings.lookAt = loadVector(document, "lookAt"));
    lightSettings.shininess = document["shininess"].GetFloat();
    lightSettings.ambientStrength = loadVector<4>(document, "ambientStrength");
    lightSettings.specularStrength = loadVector<4>(document, "specularStrength");
    lightSettings.diffuseStrength = loadVector<4>(document, "diffuseStrength");
    setOptional(lightSettings.castShadows = document["castShadows"].GetBool());
    setOptional(lightSettings.secondPoint = loadVector<3>(document, "secondPoint"));
    setOptional(lightSettings.constAtten = document["constAttenuation"].GetFloat());
    setOptional(lightSettings.linearAtten = document["linearAttenuation"].GetFloat());
    setOptional(lightSettings.quadAtten = document["quadAttenuation"].GetFloat());

    return lightSettings;
}

} // anon namespace

ResourceManager::ResourceManager(std::shared_ptr<FileSystem> fileSystem)
    : _fileSystem(std::move(fileSystem))
{
}

void ResourceManager::loadResources()
{
    LoadData data {};
    for (const auto& folder : _folderList)
    {
        FSEntityPtr fh = _fileSystem->getEntity(folder, true);

        if (fh->isDirectory())
        {
            loadDirectory(folder, data, _fileSystem);
        }
        else
        {
            loadFile(fh, data, _fileSystem);
        }
    }

    initializeResources(data);
}

void ResourceManager::initializeResources(LoadData& data, CallbackFunc callback)
{
    auto* engine = Engine::getInstance();

    uint32_t totalCount = 0;
    uint32_t currCount = 0;
    if (callback)
    {
        totalCount = data.materialsList.size() + data.engine.size() + data.shaderList.size() + data.meshList.size()
                     + data.lightList.size() + data.particleSystemList.size() + data.fontList.size();
        callback(0);
    }
    auto provideCallback = [&]()
    {
        if (callback)
        {
            ++currCount;
            callback((float)currCount / totalCount);
        }
    };

    for (auto& shaderSettings : data.shaderList)
    {
        std::shared_ptr<SVE::ShaderInfo> vertexShader = std::make_shared<SVE::ShaderInfo>(shaderSettings);
        engine->getShaderManager()->registerShader(vertexShader);
        provideCallback();
    }
    for (auto& lightSettings : data.lightList)
    {
        engine->getSceneManager()->createLight(lightSettings);
        provideCallback();
    }
    for (auto& materialSettings : data.materialsList)
    {
        if (static_cast<uint8_t>(materialSettings.loadQuality) > static_cast<uint8_t>(_maxLoadQuality))
        {
            provideCallback();
            continue;
        }
        std::shared_ptr<SVE::Material> material = std::make_shared<SVE::Material>(materialSettings);
        engine->getMaterialManager()->registerMaterial(material);
        provideCallback();
    }
    for (auto& meshLoadSettings : data.meshList)
    {
        std::shared_ptr<SVE::Mesh> mesh = std::make_shared<SVE::Mesh>(meshLoadSettings);
        engine->getMeshManager()->registerMesh(mesh);
        provideCallback();
    }
    for (auto& particleSystemSettings : data.particleSystemList)
    {
        engine->getParticleSystemManager()->registerParticleSystem(particleSystemSettings);
        provideCallback();
    }
    for (auto& font : data.fontList)
    {
        engine->getFontManager()->addFont(font);
        provideCallback();
    }
}

void ResourceManager::loadFolder(const std::string& folder, CallbackFunc callback)
{
    _folderList.push_back(folder);

    LoadData loadData {};
    loadDirectory(folder, loadData, _fileSystem);
    initializeResources(loadData, callback);
}

ResourceManager::LoadData ResourceManager::getLoadDataFromFolder(const std::string& folder, bool isFolder, const std::shared_ptr<FileSystem>& fileSystem)
{
    LoadData data {};
    FSEntityPtr fh = fileSystem->getEntity(folder, isFolder);

    if (fh->isDirectory())
    {
        loadDirectory(folder, data, fileSystem);
    }
    else if (fh->exist())
    {
        loadFile(fh, data, fileSystem);
    }
    else
    {
        throw VulkanException("Folder or file doesn't exist");
    }

    return data;
}

const std::vector<std::string> ResourceManager::getFolderList() const
{
    return _folderList;
}

std::string ResourceManager::loadFileContent(const std::string& file) const
{
    return _fileSystem->getFileContent(_fileSystem->getEntity(file));
}

void ResourceManager::loadDirectory(const std::string& directory, LoadData& loadData, const std::shared_ptr<FileSystem>& fileSystem)
{
    auto dir = fileSystem->getEntity(directory, true);
    auto fileList = fileSystem->getFileList(dir);
    for (auto& file : fileList)
    {
        loadFile(file, loadData, fileSystem);
    }
}

void ResourceManager::loadFile(FSEntityPtr file, LoadData& loadData, const std::shared_ptr<FileSystem>& fileSystem)
{
    if (file->isDirectory() || !file->exist())
        return;

    auto extension = fileSystem->getExtension(file);
    if (extension.empty())
    {
        std::cout << "Skipping unsupported file " << file->getPath() << std::endl;
        return;
    }
    auto type = fileSystem->getExtension(file).substr(1);

    static const std::map<std::string, ResourceType> resourceTypeMap {
        {"engine", ResourceType::Engine},
        {"shader", ResourceType::Shader},
        {"material", ResourceType::Material},
        {"mesh", ResourceType::Mesh},
        {"light", ResourceType::Light},
        {"particle", ResourceType::ParticleSystem},
        {"font", ResourceType::Font}
    };

    if (resourceTypeMap.find(type) == resourceTypeMap.end())
    {
        // TODO: Add logging system
        std::cout << "Skipping unsupported file " << file->getPath() << std::endl;
        return;
    }

    std::string fileContent = fileSystem->getFileContent(file);
    auto directory = fileSystem->getContainingDirectory(file);

    try
    {
        if (resourceTypeMap.find(type) == resourceTypeMap.end())
            return;
        switch (resourceTypeMap.at(type))
        {
            case ResourceType::Engine:
                loadData.engine.emplace_back(loadEngine(fileContent));
                break;
            case ResourceType::Shader:
                loadData.shaderList.emplace_back(loadShader(directory, fileContent));
                break;
            case ResourceType::Material:
                loadData.materialsList.emplace_back(loadMaterial(directory, fileContent));
                break;
            case ResourceType::Mesh:
                loadData.meshList.emplace_back(loadMesh(directory, fileContent));
                break;
            case ResourceType::Light:
                loadData.lightList.emplace_back(loadLight(fileContent));
                break;
            case ResourceType::ParticleSystem:
                loadData.particleSystemList.emplace_back(loadParticleSystem(directory, fileContent));
                break;
            case ResourceType::Font:
                loadData.fontList.emplace_back(loadFont(directory, fileContent));
                break;
        }
    } catch (const std::exception& ex)
    {
        throw VulkanException(std::string("Can't load resource file ") + file->getPath() + ": " + ex.what());
    }
}

std::string ResourceManager::getSavePath() const
{
    return _fileSystem->getSavePath();
}

void ResourceManager::setMaxMaterialLoadQuality(MaterialQuality quality)
{
    _maxLoadQuality = quality;
}

std::shared_ptr<FileSystem> ResourceManager::getFileSystem() const
{
    return _fileSystem;
}

} // namespace SVE