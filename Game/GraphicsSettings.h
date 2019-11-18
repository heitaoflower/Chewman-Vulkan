// Chewman Vulkan game
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under the MIT License
#pragma once
#include <string>

namespace Chewman
{

enum class ResolutionSettings : uint8_t
{
    Low,
    High,
    Native,
    Custom
};

enum class EffectSettings : uint8_t
{
    Low,
    High
};

enum class ParticlesSettings : uint8_t
{
    Full,
    Partial,
    None
};

constexpr uint8_t CurrentGraphicsSettingsVersion = 3;

struct GraphicsSettings
{
    uint8_t version = CurrentGraphicsSettingsVersion;
    ResolutionSettings resolution = ResolutionSettings::High;
    bool useShadows = true;
    bool useDynamicLights = true;
    ParticlesSettings particleEffects = ParticlesSettings::Full;
    EffectSettings effectSettings = EffectSettings::High;
};

std::string getResolutionText(ResolutionSettings resolutionSettings);
std::string getEffectText(EffectSettings effectSettings);
std::string getParticlesText(ParticlesSettings settings);

class GraphicsManager
{
public:
    GraphicsManager();
    GraphicsManager(const GraphicsManager&) = delete;

    void setSettings(GraphicsSettings settings);
    const GraphicsSettings& getSettings() const;

    void store();
    void load();
private:
    GraphicsSettings _currentSettings;
};

} // namespace Chewman