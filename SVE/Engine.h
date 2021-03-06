// VSE (Vulkan Simple Engine) Library
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under the MIT License

#pragma once
#include <vector>
#include <memory>
#include <chrono>
#include <SDL2/SDL.h>
#include "EngineSettings.h"
#include "SceneNode.h"
#include "FileSystem.h"

namespace SVE
{
class VulkanInstance;
class MaterialManager;
class SceneManager;
class ShaderManager;
class MeshManager;
class ResourceManager;
class ParticleSystemManager;
class PostEffectManager;
class FontManager;
class OverlayManager;
class PipelineCacheManager;

enum class CommandsType : uint8_t
{
    MainPass = 0,
    ShadowPassDirectLight,
    ShadowPassPointLights,
    // ShadowPassSpotLight,
    ReflectionPass,
    RefractionPass,
    ScreenQuadPass,
    ScreenQuadMRTPass,  // for additional bloom output
    ScreenQuadLatePass, // for particles or alike
    ScreenQuadDepthPass,
    ComputeParticlesPass,
    PostEffectPasses,
};

static const uint8_t PassCount = 9;

class Engine
{
public:
    static Engine* createInstance(SDL_Window* window, const std::string& settingsPath, std::shared_ptr<FileSystem> fileSystem, glm::ivec2 frameBufferResolution = {0, 0});
    static Engine* createInstance(SDL_Window* window, EngineSettings settings, std::shared_ptr<FileSystem> fileSystem);
    static void destroyInstance(); // debug only
    static Engine* getInstance();
    ~Engine();

    VulkanInstance* getVulkanInstance();
    MaterialManager* getMaterialManager();
    MeshManager* getMeshManager();
    ShaderManager* getShaderManager();
    SceneManager* getSceneManager();
    ResourceManager* getResourceManager();
    ParticleSystemManager* getParticleSystemManager();
    PostEffectManager* getPostEffectManager();
    FontManager* getFontManager();
    OverlayManager* getOverlayManager();
    PipelineCacheManager* getPipelineCacheManager();

    void resizeWindow();
    glm::ivec2 getRenderWindowSize();
    void finishRendering();

    void onPause();
    void onResume();

    const EngineSettings& getEngineSettings() const;
    bool isShadowMappingEnabled() const;
    bool isWaterEnabled() const;

    bool isFirstRun() const;
    void setIsFirstRun(bool value);

    CommandsType getPassType() const;
    float getTime();
    float getDeltaTime();

    void renderFrame();
    void renderFrame(float deltaTime);

private:
    Engine(SDL_Window* window, EngineSettings settings, std::shared_ptr<FileSystem> fileSystem);
    Engine(SDL_Window* window, std::shared_ptr<FileSystem> fileSystem);

    void updateTime();
    void renderFrameImpl();
private:
    static Engine* _engineInstance;
    std::unique_ptr<VulkanInstance> _vulkanInstance;
    CommandsType _commandsType = CommandsType::MainPass;
    std::unique_ptr<MaterialManager> _materialManager;
    std::unique_ptr<SceneManager> _sceneManager;
    std::unique_ptr<MeshManager> _meshManager;
    std::unique_ptr<ShaderManager> _shaderManager;
    std::unique_ptr<ResourceManager> _resourceManager;
    std::unique_ptr<ParticleSystemManager> _particleSystemManager;
    std::unique_ptr<PostEffectManager> _postEffectManager;
    std::unique_ptr<FontManager> _fontManager;
    std::unique_ptr<OverlayManager> _overlayManager;
    std::unique_ptr<PipelineCacheManager> _pipelineCacheManager;

    std::chrono::high_resolution_clock::time_point _startTime = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point _currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point _prevTime = std::chrono::high_resolution_clock::now();
    float _duration;
    float _deltaTime;
    uint64_t _frameId = 0;

    bool _isFirstRun = false;
};

} // namespace SVE