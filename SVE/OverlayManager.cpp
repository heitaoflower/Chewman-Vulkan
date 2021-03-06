// SVE (Simple Vulkan Engine) Library
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under the MIT License
#include "OverlayManager.h"

namespace SVE
{

std::shared_ptr<OverlayEntity> OverlayManager::addOverlay(OverlayInfo info)
{
    auto overlayEntity = std::make_shared<OverlayEntity>(info);
    _overlayList[info.name] = overlayEntity;
    _overlayZMap[info.zOrder].push_back(overlayEntity);

    return overlayEntity;
}

void OverlayManager::addOverlay(std::shared_ptr<OverlayEntity> entity)
{
    _overlayList[entity->getInfo().name] = entity;
    _overlayZMap[entity->getInfo().zOrder].push_back(std::move(entity));
}

void OverlayManager::removeOverlay(const std::string& name)
{
    auto overlayIter = _overlayList.find(name);
    if (overlayIter == _overlayList.end())
        return;
    auto overlay = overlayIter->second;
    auto zListIter = _overlayZMap.find(overlay->getInfo().zOrder);
    if (zListIter != _overlayZMap.end())
    {
        auto& zList = zListIter->second;
        zList.erase(std::remove(zList.begin(), zList.end(), overlay), zList.end());
    }
    _overlayList.erase(name);
}

void OverlayManager::changeOverlayOrder(const std::string& name, uint32_t newOrder)
{
    auto overlay = _overlayList.at(name);
    auto zList = _overlayZMap.at(overlay->getInfo().zOrder);
    zList.erase(std::remove(zList.begin(), zList.end(), overlay), zList.end());
    _overlayZMap[newOrder].push_back(overlay);
}

void OverlayManager::updateUniforms(UniformDataList uniformDataList) const
{
    for (auto& overlay : _overlayList)
    {
        if (overlay.second->isVisible())
            overlay.second->updateUniforms(uniformDataList);
    }
}

void OverlayManager::applyDrawingCommands(uint32_t bufferIndex, uint32_t imageIndex) const
{
    for (auto& zList : _overlayZMap)
    {
        for (auto& overlay : zList.second)
        {
            overlay->applyDrawingCommands(bufferIndex, imageIndex);
        }
    }
}

} // namespace SVE