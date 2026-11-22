#pragma once

#include "System.h"

#include "TransformSystem.h"
#include "CameraSystem.h"
#include "SpringArmSystem.h"
#include "LightSystem.h"
#include "PhysicsSystem.h"

#include "AudioSystem.h"
#include "TerrainSystem.h"
#include "GraphSystem.h"

#include "UIRenderSystem.h"
#include "MeshRenderSystem.h"
#include "SpriteRenderSystem.h"
#include "SpriteAnimationSystem.h"
#include "TilemapRenderSystem.h"
#include "AnimationSystem.h"
#include "ParticleSystem.h"
#include "ActionTimelineSystem.h"
#include "CompositeVFXSystem.h"

#if !defined(__EMSCRIPTEN__)
#include "NetworkSystem.h"
#endif
#include "KinematicCharacterSystem.h"
#include "ThirdPersonControllerSystem.h"
#include "SplineFollowSystem.h"
#include "GameplayCombatSystem.h"
#include "NavigationSystem.h"
