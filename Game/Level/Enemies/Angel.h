// Chewman Vulkan game
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under the MIT License
#pragma once
#include "DefaultEnemy.h"

namespace Chewman
{

class Angel final : public DefaultEnemy
{
public:
    Angel(GameMap* map, glm::ivec2 startPos);

protected:
    float getHeight() override;
};

} // namespace Chewman