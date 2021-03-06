// Chewman Vulkan game
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under the MIT License
#pragma once

#include "DefaultEnemy.h"
#include "Projectile.h"
#include "Game/Level/MagicEntity.h"

namespace SVE
{
class ParticleSystemEntity;
}

namespace Chewman
{

class Witch final : public DefaultEnemy
{
public:
    Witch(GameMap* map, glm::ivec2 startPos);

    void update(float deltaTime) override;

    void increaseState(EnemyState state) override;

    void resetAll() override;

private:
    enum class MagicType
    {
        Fireball,
        Teleport,
        Defrost
    };

    bool isPlayerOnLine();
    void startMagic(MagicType magicType);

    void updateMagic(float deltaTime);

    void applyMagic();
    void stopCasting();

private:
    Projectile* _projectile = nullptr;

    float _castingTime = -1.0f;
    MagicType _magicType = MagicType::Fireball;
    MoveDirection _magicDirection = MoveDirection::None;
    std::shared_ptr<SVE::MeshEntity> _castMesh;

    std::shared_ptr<SVE::MeshEntity> _teleportMesh;
    bool _teleportPSAttached = false;

    std::shared_ptr<SVE::ParticleSystemEntity> _teleportPS;
    std::shared_ptr<MagicEntity> _teleportMeshPS;
    bool _isParticlesEnabled = true;

    float _fireMagicRestore = 0.0;
    float _teleportMagicRestore = 6.5f;
};

} // namespace Chewman