// Copyright © 2026 CCP ehf.

#pragma once

#include "../renderable/primitive.h"

namespace Bones
{

const std::array<Vector4, 6> JOINT_VERTICES = {
	Vector4{ 0.0f, -0.5f, 0.0f, 0.0f },
	Vector4{ 0.5f, 0.0f, 0.0f, 0.0f },
	Vector4{ 0.0f, 0.0f, 0.5f, 0.0f },
	Vector4{ -0.5f, 0.0f, 0.0f, 0.0f },
	Vector4{ 0.0f, 0.0f, -0.5f, 0.0f },
	Vector4{ 0.0f, 0.5f, 0.0f, 0.0f },
};

const std::array<uint32_t, 24> JOINT_INDICES = {
	2,
	0,
	1,
	3,
	0,
	2,
	4,
	0,
	3,
	1,
	0,
	4,
	1,
	5,
	2,
	2,
	5,
	3,
	3,
	5,
	4,
	4,
	5,
	1,
};

const std::array<Vector4, 2> BONE_VERTICES = {
	Vector4{ 0.0f, 0.0f, 0.0f, 0.0f },
	Vector4{ 0.0f, 0.0f, 0.0f, 1.0f },
};

const std::array<uint32_t, 2> BONE_INDICES = {
	0,
	1
};

const Vector3 BONE_DEBUG_COLOR = Vector3{ 0.0f, 0.0f, 0.5f };
const Vector3 BONE_SELECTED_COLOR = Vector3{ 1.0f, 0.0f, 0.0f };

PrimitiveRenderable CreateJoint( std::shared_ptr<const Renderer> renderer, const cmf::Skeleton& skeleton, Vector3 color = BONE_DEBUG_COLOR, Vector3 selectedColor = BONE_SELECTED_COLOR );
PrimitiveRenderable CreateBone( std::shared_ptr<const Renderer> renderer, const cmf::Skeleton& skeleton, Vector3 color = BONE_DEBUG_COLOR, Vector3 selectedColor = BONE_SELECTED_COLOR );
};