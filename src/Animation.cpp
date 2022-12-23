#include "Animation.h"
#include "GLTFHelpers.h"

double GetAnimationDurationSeconds(const tinygltf::Animation& animation, const tinygltf::Model& model)
{
	double animationDuration = 0.0f;

	for (const auto& sampler : animation.samplers)
	{
		double max = model.accessors[sampler.input].maxValues[0];
		if (max > animationDuration)
		{
			animationDuration = max;
		}
	}

	return animationDuration;
}

std::vector<float> GetFixedRateWeightValues(const tinygltf::Accessor& keyframeTimesAccessor, const tinygltf::Accessor& keyframeValuesAccessor, const tinygltf::Model& model, int framesPerSecond, float parentAnimationDuration, int numMorphTargets)
{
	const float childAnimationDuration = keyframeTimesAccessor.maxValues[0];
	assert(childAnimationDuration <= parentAnimationDuration);

	const float secondsPerFrame = 1.0f / framesPerSecond;

	auto keyframeTimesData = GetAccessorBytes(keyframeTimesAccessor, model);
	std::span<float> keyframeTimes((float*)keyframeTimesData.data(), keyframeTimesAccessor.count);

	auto keyframeValuesData = GetAccessorBytes(keyframeValuesAccessor, model);
	std::span<float> keyframeValues((float*)keyframeValuesData.data(), keyframeValuesAccessor.count);

	const int numFixedRateFrames = std::ceil(parentAnimationDuration * framesPerSecond);
	const int numFixedRateKeyframes = numFixedRateFrames + 1;
	const int numWeights = numFixedRateKeyframes * numMorphTargets;

	std::vector<float> fixedFramerateWeights;
	float time = 0.0f;

	for (int frame = 0; frame < numFixedRateKeyframes; frame++, time += secondsPerFrame)
	{
		std::vector<float> weights = SampleWeightsAt(time, keyframeTimes, keyframeValues, numMorphTargets);
		fixedFramerateWeights.insert(fixedFramerateWeights.end(), weights.begin(), weights.end());
	}

	return fixedFramerateWeights;
}

std::vector<float> SampleWeightsAt(float time, std::span<float> keyframeTimes, std::span<float> weights, int numMorphTargets)
{
	std::vector<float> samples(numMorphTargets);

	if (time <= keyframeTimes.front())
	{
		for (int i = 0; i < numMorphTargets; i++)
		{
			samples[i] = weights[i];
		}
		return samples;
	}
	else if (time >= keyframeTimes.back())
	{
		for (int i = 0; i < numMorphTargets; i++)
		{
			samples[i] = weights[weights.size() - numMorphTargets + i];
		}
		return samples;
	}

	int firstGreaterThanIndex = 0;

	while (keyframeTimes[firstGreaterThanIndex] <= time)
	{
		firstGreaterThanIndex++;
	}

	float timeA = keyframeTimes[firstGreaterThanIndex - 1];
	float timeB = keyframeTimes[firstGreaterThanIndex];
	float t = (time - timeA) / (timeB - timeA);

	int indexA = firstGreaterThanIndex - 1;
	std::vector<float> weightsA(numMorphTargets);
	for (int i = 0; i < numMorphTargets; i++)
	{
		weightsA[i] = weights[numMorphTargets * indexA + i];
	}

	int indexB = firstGreaterThanIndex;
	std::vector<float> weightsB(numMorphTargets);
	for (int i = 0; i < numMorphTargets; i++)
	{
		weightsB[i] = weights[numMorphTargets * indexB + i];
	}

	for (int i = 0; i < numMorphTargets; i++)
	{
		samples[i] = weightsA[i] + (weightsB[i] - weightsA[i]) * t;
	}

	return samples;
}

std::vector<glm::mat4> ComputeGlobalMatrices(const Skeleton& skeleton, const std::vector<Entity>& entities)
{
	const int numJoints = skeleton.joints.size();
	std::vector<glm::mat4> globalMatrices(numJoints);

	// Set all joint matrices to their local matrices
	for (int i = 0; i < numJoints; i++)
	{
		globalMatrices[i] = entities[skeleton.joints[i].entityIndex].transform.GetMatrix();
	}

	for (int i = 1; i < numJoints; i++)
	{
		auto& joint = skeleton.joints[i];
		if (joint.parent >= 0)
		{
			globalMatrices[i] = globalMatrices[joint.parent] * globalMatrices[i];
		}
	}

	return globalMatrices;
}

std::vector<glm::mat4> ComputeSkinningMatrices(const Skeleton& skeleton, const std::vector<Entity>& entities)
{
	auto skinningMatrices = ComputeGlobalMatrices(skeleton, entities);
	const int numJoints = skinningMatrices.size();

	for (int i = 0; i < numJoints; i++)
	{
		const auto localToJoint = glm::mat4(skeleton.joints[i].localToJoint);
		skinningMatrices[i] = skinningMatrices[i] * localToJoint;
	}

	return skinningMatrices;
}
