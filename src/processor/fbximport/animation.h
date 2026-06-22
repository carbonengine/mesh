// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/cmf.h"
#include "cmf/memallocator.h"
#include "ufbx.h"
#include "skeleton.h"
#include "transform.h"
#include "options.h"

/**
* @brief Imports all animations from an FBX scene into animation channels and curves.
*
* This function iterates over all animation stacks in the FBX scene and imports both skeletal and morph target animations.
*
* @param scene The FBX scene containing the animation data.
* @param boneMap A map of bones in the scene.
* @param options Options for importing animations.
* @param allocator The memory allocator used for allocating curve data.
* @param system The coordinate system to use for transforming animation data.
* @return A span containing the imported animations.
*/
cmf::Span<cmf::Animation> ImportAnimations( const ufbx_scene& scene, const BoneMap& boneMap, const AnimationImportOptions& options, cmf::MemoryAllocator& allocator, const CoordinateSystem& system );
