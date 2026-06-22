// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/cmf.h"
#include "cmf/memallocator.h"
#include "ufbx.h"
#include "options.h"
#include "transform.h"
#include <map>


/** @brief Information about a bone in a skeleton.
 */
struct BoneInfo
{
	// The index of the skeleton this bone belongs to.
	uint32_t skeletonIndex = 0;
	// Whether this bone is the root bone of its skeleton.
	bool isRoot = false;
};

using BoneMap = std::map<const ufbx_node*, BoneInfo>;

/** @brief Imports skeletons from an FBX scene.
 * @param scene The FBX scene containing the skeletons to import.
 * @param options The options for importing the skeletons.
 * @param allocator The memory allocator to use for allocations.
 * @param system The coordinate system to use for transformations.
 * @return A pair containing a span of imported CMF skeletons and a map of bones to their corresponding skeleton index and if the bone is a root.
 */
std::pair<cmf::Span<cmf::Skeleton>, BoneMap> ImportSkeletons( const ufbx_scene& scene, const SkeletonImportOptions& options, cmf::MemoryAllocator& allocator, const CoordinateSystem& system );
