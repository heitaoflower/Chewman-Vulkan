// Chewman Vulkan game
// Copyright (c) 2018-2019, Igor Barinov
// Licensed under the MIT License
#include "Player.h"

#include <SVE/Engine.h>
#include <SVE/SceneManager.h>
#include <SVE/MeshEntity.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Game/Game.h"
#include "GameMap.h"
#include "GameUtils.h"
#include "CustomEntity.h"

#include <SDL2/SDL_events.h>

namespace Chewman
{

namespace
{

std::shared_ptr<SVE::LightNode> addLightEffect(SVE::Engine* engine)
{
    SVE::LightSettings lightSettings {};
    lightSettings.lightType = SVE::LightType::PointLight;
    lightSettings.castShadows = false;
    lightSettings.diffuseStrength = glm::vec4(1.0, 1.0, 0.5, 1.0);
    lightSettings.specularStrength = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    lightSettings.ambientStrength = { 0.3f, 0.3f, 0.3f, 1.0f };
    lightSettings.shininess = 16;
    lightSettings.constAtten = 1.0f * 1.8f;
    lightSettings.linearAtten = 0.35f * 0.15f;
    lightSettings.quadAtten = 0.44f * 0.15f;

    if (Game::getInstance()->getGraphicsManager().getSettings().dynamicLights == LightSettings::Simple)
        lightSettings.isSimple = true;

    auto lightNode = std::make_shared<SVE::LightNode>(lightSettings);
    lightNode->setNodeTransformation(glm::translate(glm::mat4(1), glm::vec3(0, 2.5, 0)));

    return lightNode;
}

} // anon namespace

Player::Player(GameMap* gameMap, glm::ivec2 startPos)
    : _mapTraveller(std::make_shared<MapTraveller>(gameMap, startPos))
    , _gameMap(gameMap)
    , _startPos(startPos)
{
    _mapTraveller->setWaterAccessible(true);

    auto* engine = SVE::Engine::getInstance();

    auto realMapPos = _mapTraveller->getRealPosition();
    auto position = glm::vec3(realMapPos.y, 0, -realMapPos.x);

    _rootNode = engine->getSceneManager()->createSceneNode();
    auto transform = glm::translate(glm::mat4(1), position);

    _rootNode->setNodeTransformation(transform);
    gameMap->mapNode->attachSceneNode(_rootNode);

    _rotateNode = engine->getSceneManager()->createSceneNode();
    _rootNode->attachSceneNode(_rotateNode);

    _trashmanEntity = std::make_shared<SVE::MeshEntity>("trashman");
    _trashmanEntity->setMaterial("Yellow");
    _rotateNode->attachEntity(_trashmanEntity);

    if (Game::getInstance()->getGraphicsManager().getSettings().dynamicLights != LightSettings::Off)
    {
        _lightNode = addLightEffect(engine);
        if (gameMap->isNight)
            _rootNode->attachSceneNode(_lightNode);
    }

    createAppearEffect();
    createDisappearEffect();
    createPowerUpEffect();
}

void Player::update(float deltaTime)
{
    if (_followMode)
    {
        if (!_isDying)
        {
            updateMovement(deltaTime);

            if (_isCameraFollow)
            {
                auto camera = SVE::Engine::getInstance()->getSceneManager()->getMainCamera();
                camera->setParent(_rootNode);
                auto cameraStyle = Game::getInstance()->getGameSettingsManager().getSettings().cameraStyle;
                camera->setLookAt(getCameraPos(cameraStyle), glm::vec3(0), glm::vec3(0, 1, 0));
            }

            if (_appearing)
            {
                _appearTime += deltaTime;
                updateAppearEffect();
                if (_appearTime > 0.5f)
                {
                    showAppearEffect(false);
                }
                if (_appearTime > 1.0f)
                {
                    _appearing = false;
                    _trashmanEntity->setMaterial("Yellow");
                    _trashmanEntity->setRenderLast(false);
                    _trashmanEntity->setCastShadows(true);
                }

            }
            if (_powerUpTime > 0)
            {
                _powerUpTime-= deltaTime;

                //const auto rotateAngle = -90.0f * static_cast<uint8_t>(_mapTraveller->getCurrentDirection());
                //_powerUpEffectNode->setNodeTransformation(glm::rotate(glm::mat4(1), glm::radians(rotateAngle), glm::vec3(0, 1, 0)));
            } else {
                _rootNode->detachSceneNode(_powerUpEffectNode);
            }
        }
    }

    const auto realMapPos = _mapTraveller->getRealPosition();
    const auto position = glm::vec3(realMapPos.y, 0, -realMapPos.x);
    _rootNode->setNodeTransformation(glm::translate(glm::mat4(1), position));
    const auto rotateAngle = 90.0f * static_cast<uint8_t>(_mapTraveller->getCurrentDirection());
    _rotateNode->setNodeTransformation(glm::rotate(glm::mat4(1), glm::radians(rotateAngle), glm::vec3(0, 1, 0)));
}

void Player::tryApplyShift()
{
    if (_isDirectionChanged)
        return; // specific direction already applied

    if (!_mapTraveller->isTargetReached() && _mapTraveller->isCloseToTurn()
        && !isOrthogonalDirection(_nextMove, _mapTraveller->getCurrentDirection())
        && _mapTraveller->isMovePossible(_nextMove))
    {
        _mapTraveller->resetPositionWithShift();
    }
}

void Player::processInput(const SDL_Event& event)
{
    if (event.type == SDL_KEYUP)
    {
        if (event.key.keysym.sym == SDLK_f)
        {
            _followMode = !_followMode;
        }
        if (event.key.keysym.sym == SDLK_g)
        {
            // for safety
            _followMode = !_followMode;
        }
    }

    if (_followMode)
    {
        const Uint8* keystates = SDL_GetKeyboardState(nullptr);
        if (keystates[SDL_SCANCODE_A])
            _nextMove = MoveDirection::Down;
        if (keystates[SDL_SCANCODE_D])
            _nextMove = MoveDirection::Up;
        if (keystates[SDL_SCANCODE_W])
            _nextMove = MoveDirection::Right;
        if (keystates[SDL_SCANCODE_S])
            _nextMove = MoveDirection::Left;

        auto controllerType = Game::getInstance()->getGameSettingsManager().getSettings().controllerType;

        if (controllerType == ControllerType::Swipe)
        {
            if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                _isSliding = true;
                _startSlideX = event.button.x;
                _startSlideY = event.button.y;
            }
            if (event.type == SDL_MOUSEMOTION && _isSliding)
            {
                auto windowSize = SVE::Engine::getInstance()->getRenderWindowSize();
                if (abs(_startSlideX - event.button.x) < windowSize.x * 0.015
                    || abs(_startSlideY - event.button.y) < windowSize.y * 0.015)
                {
                    return;
                }

                if (abs(event.motion.x - _startSlideX) > abs(event.motion.y - _startSlideY))
                {
                    if (_startSlideX > event.motion.x)
                        _nextMove = MoveDirection::Down;
                    else
                        _nextMove = MoveDirection::Up;
                } else
                {
                    if (_startSlideY > event.motion.y)
                        _nextMove = MoveDirection::Right;
                    else
                        _nextMove = MoveDirection::Left;
                }

                tryApplyShift();
            }
            if (event.type == SDL_MOUSEBUTTONUP)
            {
                auto windowSize = SVE::Engine::getInstance()->getRenderWindowSize();
                _isSliding = false;
                if (abs(_startSlideX - event.button.x) < windowSize.x * 0.001
                    || abs(_startSlideY - event.button.y) < windowSize.y * 0.001)
                {
                    return;
                }

                if (abs(event.button.x - _startSlideX) > abs(event.button.y - _startSlideY))
                {
                    if (_startSlideX > event.button.x)
                        _nextMove = MoveDirection::Down;
                    else
                        _nextMove = MoveDirection::Up;
                } else
                {
                    if (_startSlideY > event.button.y)
                        _nextMove = MoveDirection::Right;
                    else
                        _nextMove = MoveDirection::Left;
                }

                tryApplyShift();
            }
        }

        if (controllerType == ControllerType::Accelerometer)
        {
#ifdef __ANDROID__
            if (event.type == SDL_SENSORUPDATE)
            {
                SDL_Sensor* sensor = SDL_SensorFromInstanceID(event.sensor.which);

                if (sensor && SDL_SensorGetType(sensor) == SDL_SENSOR_ACCEL)
                {
                    if (!_basisInitialized)
                    {
                        _basisInitialized = true;
                        _accelBasis = {event.sensor.data[0], event.sensor.data[1], event.sensor.data[2]};
                        _accelPrev = _accelBasis;
                    }
                    else
                    {
                        auto curNext = _nextMove;
                        if (fabs(event.sensor.data[1] - _accelBasis.y) > 2.0 && fabs(event.sensor.data[2] - _accelBasis.z) > 2.0)
                        {
                            if (fabs(event.sensor.data[1] - _accelPrev.y) > fabs(event.sensor.data[2] - _accelPrev.z))
                            {
                                if (event.sensor.data[1] - _accelBasis.y < -2.0)
                                    _nextMove = MoveDirection::Down;
                                else if (event.sensor.data[1] - _accelBasis.y > 2.0f)
                                    _nextMove = MoveDirection::Up;
                            } else {
                                if (event.sensor.data[2] - _accelBasis.z < -2.0f)
                                    _nextMove = MoveDirection::Left;
                                else if (event.sensor.data[2] - _accelBasis.z > 2.0f)
                                    _nextMove = MoveDirection::Right;
                            }
                        }
                        if (fabs(event.sensor.data[1] - _accelBasis.y) > fabs(event.sensor.data[2] - _accelBasis.z))
                        {
                            if (event.sensor.data[1] - _accelBasis.y < -2.0)
                                _nextMove = MoveDirection::Down;
                            else if (event.sensor.data[1] - _accelBasis.y > 2.0f)
                                _nextMove = MoveDirection::Up;
                        } else
                        {
                            if (event.sensor.data[2] - _accelBasis.z < -2.0f)
                                _nextMove = MoveDirection::Left;
                            else if (event.sensor.data[2] - _accelBasis.z > 2.0f)
                                _nextMove = MoveDirection::Right;
                        }

                        if (curNext != _nextMove)
                        {
                            _accelPrev = {event.sensor.data[0], event.sensor.data[1], event.sensor.data[2]};
                        }
                    }
                }

                tryApplyShift();
            }
#endif
        }

        if (controllerType == ControllerType::Joystick)
        {
            if (event.type == SDL_MOUSEMOTION)
            {
                tryApplyShift();
            }
        }
    }
}

void Player::updateMovement(float deltaTime)
{
    if (_mapTraveller->isTargetReached())
    {
        if (_mapTraveller->isMovePossible(_nextMove))
        {
            _isDirectionChanged = _nextMove != _mapTraveller->getCurrentDirection();
            if (_isDirectionChanged)
                _mapTraveller->removeAutoShift();
            _mapTraveller->move(_nextMove);
        } else {
            auto current = _mapTraveller->getCurrentDirection();
            if (_mapTraveller->isMovePossible(current))
                _mapTraveller->move(current);
            _isDirectionChanged = false;
        }
    }
    else if (_nextMove != _mapTraveller->getCurrentDirection() && isAntiDirection(_nextMove, _mapTraveller->getCurrentDirection()))
    {
        if (_mapTraveller->isMovePossible(_nextMove))
            _mapTraveller->move(_nextMove);
    }
    _mapTraveller->update(deltaTime);
}

std::shared_ptr<MapTraveller> Player::getMapTraveller()
{
    return _mapTraveller;
}

void Player::resetPosition()
{
    _trashmanEntity->setMaterial("YellowAppearTrashman");
    _trashmanEntity->setAnimationState(SVE::AnimationState::Play);
    _trashmanEntity->setRenderLast(true);
    _trashmanEntity->setCastShadows(false);
    _trashmanEntity->resetTime(0.3f);
    _mapTraveller->setPosition(_startPos);
    _nextMove = MoveDirection::None;

    showDisappearEffect(false);
    showAppearEffect(true);
}

void Player::playDeathAnimation()
{
    showDisappearEffect(true);
    _trashmanEntity->setMaterial("YellowBurnTrashman");
    _trashmanEntity->setAnimationState(SVE::AnimationState::Pause);
    _trashmanEntity->setRenderLast();
    _trashmanEntity->setCastShadows(false);
    _trashmanEntity->resetTime();
}

void Player::resetPlaying()
{
    _appearing = true;
    _appearTime = 0.0f;
    _isDying = false;
}

void Player::createAppearEffect()
{
    auto* engine = SVE::Engine::getInstance();
    auto color = glm::vec3(1.0, 1.0, 0.5);

    _appearNode = engine->getSceneManager()->createSceneNode();
    std::shared_ptr<SVE::ParticleSystemEntity> starsPS = std::make_shared<SVE::ParticleSystemEntity>("PowerUp");
    starsPS->getMaterialInfo()->diffuse = glm::vec4(color, 0.6f);
    _appearNode->attachEntity(starsPS);

    _appearNodeGlow = engine->getSceneManager()->createSceneNode();
    _appearNode->attachSceneNode(_appearNodeGlow);
    {
        std::shared_ptr<SVE::MeshEntity> teleportCircleEntity = std::make_shared<SVE::MeshEntity>("cylinder");
        teleportCircleEntity->setMaterial("TeleportCircleMaterial");
        teleportCircleEntity->setRenderLast();
        teleportCircleEntity->setCastShadows(false);
        teleportCircleEntity->getMaterialInfo()->diffuse = { color, 1.0f };
        _appearNodeGlow->attachEntity(teleportCircleEntity);
    }
}

void Player::showAppearEffect(bool show)
{
    if (show)
    {
        _rootNode->attachSceneNode(_appearNode);
    } else {
        _rootNode->detachSceneNode(_appearNode);
    }
}

void Player::showDisappearEffect(bool show)
{
    if (show)
    {
        _rootNode->attachSceneNode(_disappearNode);
    } else {
        _rootNode->detachSceneNode(_disappearNode);
    }
}

void Player::updateAppearEffect()
{
    auto updateNode = [](std::shared_ptr<SVE::SceneNode>& node, float time)
    {
        auto nodeTransform = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        node->setNodeTransformation(nodeTransform);
    };

    updateNode(_appearNodeGlow, _appearTime * 5);
}

void Player::createPowerUpEffect()
{
    auto* engine = SVE::Engine::getInstance();
    auto color = glm::vec3(0.5, 0.5, 1.0);

    _powerUpEffectNode = engine->getSceneManager()->createSceneNode();
    _powerUpPS = std::make_shared<SVE::ParticleSystemEntity>("PowerUp");
    _powerUpPS->getMaterialInfo()->diffuse = glm::vec4(color, 0.6f);
    _powerUpEffectNode->attachEntity(_powerUpPS);

    auto spiralNode = engine->getSceneManager()->createSceneNode();
    spiralNode->setNodeTransformation(glm::translate(glm::mat4(1), glm::vec3(0, 1, 0)));
    _powerUpEffectNode->attachSceneNode(spiralNode);
    _powerUpEntity = std::make_shared<SVE::MeshEntity>("spiral");
    _powerUpEntity->setMaterial("PowerUpMaterial");
    //powerUpNode->setRenderLast();
    _powerUpEntity->setCastShadows(false);
    _powerUpEntity->getMaterialInfo()->diffuse = {1.0, 1.0, 1.0, 1.0f };
    spiralNode->attachEntity(_powerUpEntity);
}

void Player::playPowerUpAnimation()
{
    _powerUpEntity->setMaterial("PowerUpMaterial");
    _powerUpEntity->getMaterialInfo()->diffuse = {1.0, 1.0, 1.0, 1.0f };
    _powerUpPS->getMaterialInfo()->diffuse = glm::vec4(0.5f, 0.5f, 1.0f, 0.6f);
    _rootNode->attachSceneNode(_powerUpEffectNode);
    _powerUpEntity->resetTime();
    _powerUpTime = 1.2f;
}

void Player::playPowerDownAnimation()
{
    _powerUpEntity->setMaterial("PowerDownMaterial");
    _powerUpEntity->getMaterialInfo()->diffuse = {0.4, 0.4, 0.4, 0.9f };
    _powerUpPS->getMaterialInfo()->diffuse = glm::vec4(0.1f, 0.1f, 0.1f, 0.9f);
    _rootNode->attachSceneNode(_powerUpEffectNode);
    _powerUpEntity->resetTime();
    _powerUpTime = 1.2f;
}

void Player::setIsDying(bool isDying)
{
    _isDying = isDying;
}

bool Player::isDying()
{
    return _isDying;
}

void Player::enableLight(bool enable)
{
    if (enable)
        _rootNode->attachSceneNode(_lightNode);
    else
        _rootNode->detachSceneNode(_lightNode);
}

void Player::setNextMove(MoveDirection direction)
{
    if (_followMode)
    {
        _nextMove = direction;
    }
}

MoveDirection Player::getNextMove() const
{
    return _nextMove;
}

void Player::createDisappearEffect()
{
    auto* engine = SVE::Engine::getInstance();
    auto color = glm::vec3(1.0, 0.5, 0.0);

    _disappearNode = engine->getSceneManager()->createSceneNode();

   if (Game::getInstance()->getGraphicsManager().getSettings().particleEffects != ParticlesSettings::None)
   {
       std::shared_ptr<SVE::ParticleSystemEntity> disappearPS = std::make_shared<SVE::ParticleSystemEntity>("Disappear");
       disappearPS->getMaterialInfo()->diffuse = glm::vec4(color, 1.5f);
       _disappearNode->attachEntity(disappearPS);
   } else {
       MagicInfo info {};
       info.color = color;
       info.maxParticles = 500;
       info.ratio = 10.0;
       info.radius = 0.7f;
       info.particleSize = 0.2;
       auto magicEntity = std::make_shared<MagicEntity>("MagicMeshParticleMaterial", info);
       _disappearNode->attachEntity(magicEntity);
   }
}

void Player::setCameraFollow(bool value)
{
    _isCameraFollow = value;
}

} // namespace Chewman