#pragma once

#include <array>
#include "Entity.h"
#include "GLTFHelpers.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tiny_gltf/tiny_gltf.h>
#include <span>
#include "Skeleton.h"
#include <vector>

enum class InterpolationType
{
	LINEAR,
	STEP,
	CUBICSPLINE
};

template<typename T>
struct PropertyAnimation
{
	std::vector<T> values;
	std::vector<float> times;
	InterpolationType method;
};

struct EntityAnimation
{
	Entity* entity;
	PropertyAnimation<glm::vec3> translations;
	PropertyAnimation<glm::vec3> scales;
	PropertyAnimation<glm::quat> rotations;
	PropertyAnimation<float> weights;
};

struct Animation
{
	std::vector<EntityAnimation> entityAnimations;
	float durationSeconds;
};

double GetAnimationDurationSeconds(const tinygltf::Animation& animation, const tinygltf::Model& model);
std::vector<float> SampleWeightsAt(const PropertyAnimation<float>& animation, float normalizedTime, int numMorphTargets = 2);
std::vector<glm::mat4> ComputeGlobalMatrices(const Skeleton& skeleton, const std::vector<Entity>& entites);
std::vector<glm::mat4> ComputeSkinningMatrices(const Skeleton& skeleton, const std::vector<Entity>& entities);

// Use for translation, scale, or rotation. For translation or scale, lerp is used. For rotation (quaternions),
// slerp is used. If time lies outside the time span, the nearest keyframe's value is returned and no interpolation is used
template<typename T>
inline T SampleAt(const PropertyAnimation<T>& animation, float normalizedTime)
{
	constexpr bool translationOrScale = std::is_same<T, glm::vec3>::value;
	constexpr bool rotation = std::is_same<T, glm::quat>::value;
	static_assert(translationOrScale || rotation);

	if (normalizedTime < animation.times.front())
	{
		if (animation.method != InterpolationType::CUBICSPLINE)
		{
			return animation.values.front();
		}

		return animation.values[1]; // First value comes after in-tangent at index 0 for cubic spline interpolation
	}
	else if (normalizedTime > animation.times.back())
	{
		if (animation.method != InterpolationType::CUBICSPLINE)
		{
			return animation.values.back();
		}

		return animation.values[animation.values.size() - 2]; // Last value comes before out-tangent 
	}

	int nextKeyframeIndex = 0;

	while (animation.times[nextKeyframeIndex] <= normalizedTime)
	{
		nextKeyframeIndex++;
	}

	assert(nextKeyframeIndex > 0);

	float previousTime = animation.times[nextKeyframeIndex - 1];
	if (animation.method == InterpolationType::STEP)
	{
		return animation.values[nextKeyframeIndex - 1];
	}
	float nextTime = animation.times[nextKeyframeIndex];
	float deltaTime = nextTime - previousTime;
	float t = (normalizedTime - previousTime) / deltaTime;

	if (animation.method == InterpolationType::LINEAR)
	{
		const T& previousValue = animation.values[nextKeyframeIndex - 1];
		const T& nextValue = animation.values[nextKeyframeIndex];

		if constexpr (translationOrScale)
		{
			return previousValue + (nextValue - previousValue) * t;
		}
		else
		{
			return glm::slerp(previousValue, nextValue, t);
		}
	}
	else // cubic spline interpolation https://github.khronos.org/glTF-Tutorials/gltfTutorial/gltfTutorial_007_Animations.html#cubic-spline-interpolation
	{
		int previousValueIndex = (nextKeyframeIndex - 1) * 3 + 1;
		int previousOutputTangentIndex = previousValueIndex + 1;

		int nextInputTangentIndex = nextKeyframeIndex * 3;
		int nextValueIndex = nextInputTangentIndex + 1;

		const T& previousValue = animation.values[previousValueIndex];
		const T& nextValue = animation.values[nextValueIndex];
		const T previousOutTangent = deltaTime * animation.values[previousOutputTangentIndex];
		const T nextInTangent = deltaTime * animation.values[nextInputTangentIndex];

		float t2 = t * t;
		float t3 = t2 * t;

		if constexpr (translationOrScale)
		{
			return previousValue * (2 * t3 - 3 * t2 + 1) + previousOutTangent * (t3 - 2 * t2 + t) + nextValue * (-2 * t3 + 3 * t2) + nextInTangent * (t3 - t2);
		}
		else
		{
			return glm::normalize(previousValue * (2 * t3 - 3 * t2 + 1) + previousOutTangent * (t3 - 2 * t2 + t) + nextValue * (-2 * t3 + 3 * t2) + nextInTangent * (t3 - t2));
		}
	}
}