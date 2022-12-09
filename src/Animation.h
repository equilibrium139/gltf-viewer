#pragma once

#include "Entity.h"

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tiny_gltf/tiny_gltf.h>
#include <span>
#include <vector>

struct Animation
{
	struct EntityAnimation
	{
		Entity* entity;
		std::vector<glm::vec3> translations;
		std::vector<glm::vec3> scales;
		std::vector<glm::quat> rotations;
		std::vector<float> weights;
	};
	std::vector<EntityAnimation> entityAnimations;
	float durationSeconds;
	int	framesPerSecond = 30;
};

double GetAnimationDurationSeconds(const tinygltf::Animation& animation, const tinygltf::Model& model);
std::vector<float> GetFixedRateWeightValues(const tinygltf::Accessor& keyframeTimesAccessor, const tinygltf::Accessor& keyframeValuesAccessor, const tinygltf::Model& model, int framesPerSecond, float parentAnimationDuration, int numMorphTargets = 2);
std::vector<float> SampleWeightsAt(float time, std::span<float> keyframeTimes, std::span<float> weights, int numMorphTargets = 2);

// Use for translation, scale, or rotation. For translation or scale, lerp is used. For rotation (quaternions),
// slerp is used. If time lies outside the time span, the nearest keyframe's value is returned and no interpolation is used
template<typename T>
inline T SampleAt(float time, std::span<float> keyframeTimes, std::span<T> keyframeValues)
{
	constexpr bool translationOrScale = std::is_same<T, glm::vec3>::value;
	constexpr bool rotation = std::is_same<T, glm::quat>::value;
	static_assert(translationOrScale || rotation);

	if (time < keyframeTimes.front())
	{
		return keyframeValues.front();
	}
	else if (time > keyframeTimes.back())
	{
		return keyframeValues.back();
	}

	int firstGreaterThanIndex = 0;

	while (keyframeTimes[firstGreaterThanIndex] <= time)
	{
		firstGreaterThanIndex++;
	}

	assert(firstGreaterThanIndex > 0);

	float timeA = keyframeTimes[firstGreaterThanIndex - 1];
	float timeB = keyframeTimes[firstGreaterThanIndex];
	float t = (time - timeA) / (timeB - timeA);

	const T& valueA = keyframeValues[firstGreaterThanIndex - 1];
	const T& valueB = keyframeValues[firstGreaterThanIndex];

	if constexpr (translationOrScale)
	{
		return t * valueA + (1.0f - t) * valueB;
	}
	else
	{
		return glm::slerp(valueA, valueB, t);
	}
}

// Use for channels that animate translation, scale, or rotation
template<typename T>
std::vector<T> GetFixedRateTRSValues(const tinygltf::Accessor& keyframeTimesAccessor, const tinygltf::Accessor& keyframeValuesAccessor, const tinygltf::Model& model, int framesPerSecond, float parentAnimationDuration)
{
	const float childAnimationDuration = keyframeTimesAccessor.maxValues[0];
	assert(childAnimationDuration <= parentAnimationDuration);

	const float secondsPerFrame = 1.0f / framesPerSecond;
	std::span<float> keyframeTimes = GetAccessorDataView<float>(keyframeTimesAccessor, model);
	std::span<T> keyframeValues = GetAccessorDataView<T>(keyframeValuesAccessor, model);

	const int numFixedRateKeyframes = parentAnimationDuration * framesPerSecond + 1;
	std::vector<T> fixedRateValues(numFixedRateKeyframes);
	float time = 0.0f;

	for (int frame = 0; frame < numFixedRateKeyframes; frame++, time += secondsPerFrame)
	{
		fixedRateValues[frame] = SampleAt(time, keyframeTimes, keyframeValues);
	}

	return fixedRateValues;
}
