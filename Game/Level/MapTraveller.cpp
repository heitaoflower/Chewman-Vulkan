// Chewman Vulkan game
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under the MIT License
#include "MapTraveller.h"
#include "GameMap.h"
#include "SVE/Engine.h"

namespace Chewman
{
namespace
{

constexpr float TurnDistance = CellSize / 3.6f;

void moveTo(MoveDirection dir, glm::ivec2& mapPosition)
{
    switch (dir)
    {
        case MoveDirection::Left:
            mapPosition.x--;
            break;
        case MoveDirection::Right:
            mapPosition.x++;
            break;
        case MoveDirection::Up:
            mapPosition.y++;
            break;
        case MoveDirection::Down:
            mapPosition.y--;
            break;
    }
}

} // anon namespace

MapTraveller::MapTraveller(GameMap* map, glm::ivec2 startPosMap, float moveSpeed)
    : MapTraveller(map, toRealPos(startPosMap), moveSpeed)
{
}

MapTraveller::MapTraveller(GameMap* map, glm::vec2 startPosReal, float moveSpeed)
    : _map(map)
    , _position(startPosReal)
    , _target(startPosReal)
    , _moveSpeed(moveSpeed)
    , _direction(MoveDirection::None)
{
}

bool MapTraveller::tryMove(MoveDirection dir)
{
    if (!isMovePossible(dir))
        return false;
    move(dir);
    return true;
}

void MapTraveller::move(MoveDirection dir)
{
    _speed = {0.0f, 0.0f};
    switch (dir)
    {
        case MoveDirection::Left:
            _speed.x = -_moveSpeed;
            break;
        case MoveDirection::Right:
            _speed.x = _moveSpeed;
            break;
        case MoveDirection::Up:
            _speed.y = _moveSpeed;
            break;
        case MoveDirection::Down:
            _speed.y = -_moveSpeed;
            break;
    }

    _direction = dir;
    _targetReached = false;

    _lastTarget = _target;
    auto mapPosition = getMapPosition();
    moveTo(dir, mapPosition);
    _start = _position;
    _target = toRealPos(mapPosition);
}

bool MapTraveller::isMovePossible(MoveDirection dir)
{
    auto mapPosition = getMapPosition();
    moveTo(dir, mapPosition);
    return isFreePosition(mapPosition);
}


void MapTraveller::update(float deltaTime)
{
    _position += _speed * deltaTime;

    if (_useShift)
    {
        int zeroed = 0;
        float speed = deltaTime * MoveSpeed * 0.5f;
        if (fabs(_shift.x) > speed)
            _shift.x += (_shift.x > 0) ? -speed : speed;
        else
        {
            _shift.x = 0;
            zeroed++;
        }

        if (fabs(_shift.y) > speed)
            _shift.y += (_shift.y > 0) ? -speed : speed;
        else
        {
            _shift.y = 0;
            zeroed++;
        }

        if (zeroed == 2)
        {
            _useShift = false;
            _customShift = false;
        }
    }
    auto distance = _target - _position;
    if (!_targetReached && glm::dot(distance, _speed) <= 0)
    {
        if (_direction != MoveDirection::None && isMovePossible(_direction))
        {
            _shift = getRealPosition() - _target;
            _useShift = true;
            //_customShift = false;
        }
        _position = _target;

        _speed = { 0, 0 };
        _targetReached = true;
    }
}

glm::ivec2 MapTraveller::getMapPosition() const
{
    return glm::ivec2(_position.x / CellSize + 0.5, _position.y / CellSize + 0.5);
}

glm::ivec2 MapTraveller::getMapPosition(glm::vec2 realPos)
{
    return glm::ivec2(realPos.x / CellSize + 0.5, realPos.y / CellSize + 0.5);
}

glm::vec2 MapTraveller::getRealPosition() const
{
    if (_useShift)
        return _position + _shift;
    return _position;
}

float MapTraveller::getSpeed() const
{
    return _moveSpeed;
}

bool MapTraveller::isFreePosition(glm::ivec2 position)
{
    if (position.x < 0 || position.x >= _map->mapData.size() ||
        position.y < 0 || position.y >= _map->mapData.front().size())
        return false;

    switch (_map->mapData[position.x][position.y].cellType)
    {
        case CellType::Floor:
            return true;
        case CellType::Liquid:
            return _waterAllowed;
        case CellType::Wall:
        case CellType::InvisibleWallWithFloor:
        case CellType::InvisibleWallEmpty:
            return _wallAllowed;
    }

    return false;
}

void MapTraveller::setSpeed(float speed)
{
    _moveSpeed = speed;
}

bool MapTraveller::isTargetReached() const
{
    return _targetReached;
}

glm::vec2 MapTraveller::getStartPos() const
{
    return _start;
}

glm::vec2 MapTraveller::getTargetPos() const
{
    return _target;
}

void MapTraveller::setTargetPos(glm::vec2 pos)
{
    _target = pos;
}



GameMap* MapTraveller::getGameMap() const
{
    return _map;
}

void MapTraveller::setWaterAccessible(bool accessible)
{
    _waterAllowed = accessible;
}

void MapTraveller::setWallAccessible(bool accessible)
{
    _wallAllowed = accessible;
}

void MapTraveller::setAffectDistance(float distance)
{
    _affectDistance = distance;
}

MoveDirection MapTraveller::getCurrentDirection() const
{
    return _direction;
}

void MapTraveller::setPosition(glm::ivec2 position)
{
    _position = toRealPos(position);
    _target = _position;
    _targetReached = true;
    _direction = MoveDirection::None;
}

void MapTraveller::resetPositionWithShift()
{
    _shift = getRealPosition() - _lastTarget;
    _position = _lastTarget;
    _useShift = true;
    _targetReached = true;
    _customShift = true;
    _speed = {0, 0};
}

void MapTraveller::removeAutoShift()
{
    if (!_customShift)
    {
        _useShift = false;
        _shift = {0, 0};
    }
}

bool MapTraveller::isCloseToAffect(glm::vec2 pos) const
{
    return glm::distance(_position, pos) < _affectDistance;
}

bool MapTraveller::isCloseToTurn() const
{
    return glm::distance(_position, _lastTarget) < TurnDistance;
}

glm::vec2 MapTraveller::toRealPos(glm::ivec2 pos)
{
    return glm::vec2(CellSize * pos.x,  CellSize * pos.y);
}

} // namespace Chewman