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

std::vector<float> SampleWeightsAt(const PropertyAnimation<float>& animation, float normalizedTime, int numMorphTargets)
{
	assert(animation.method == InterpolationType::LINEAR); // for now, too lazy

	std::vector<float> samples(numMorphTargets);

	if (normalizedTime <= animation.times.front())
	{
		for (int i = 0; i < numMorphTargets; i++)
		{
			samples[i] = animation.values[i];
		}
		return samples;
	}
	else if (normalizedTime >= animation.times.back())
	{
		for (int i = 0; i < numMorphTargets; i++)
		{
			samples[i] = animation.values[animation.values.size() - numMorphTargets + i];
		}
		return samples;
	}

	int nextKeyframeTimeIndex = 0;

	while (animation.times[nextKeyframeTimeIndex] <= normalizedTime)
	{
		nextKeyframeTimeIndex++;
	}

	float previousKeyframeTime = animation.times[nextKeyframeTimeIndex - 1];
	float nextKeyframeTime = animation.times[nextKeyframeTimeIndex];
	float t = (normalizedTime - previousKeyframeTime) / (nextKeyframeTime - previousKeyframeTime);

	int indexA = nextKeyframeTimeIndex - 1;
	std::vector<float> weightsA(numMorphTargets);
	for (int i = 0; i < numMorphTargets; i++)
	{
		weightsA[i] = animation.values[numMorphTargets * indexA + i];
	}

	int indexB = nextKeyframeTimeIndex;
	std::vector<float> weightsB(numMorphTargets);
	for (int i = 0; i < numMorphTargets; i++)
	{
		weightsB[i] = animation.values[numMorphTargets * indexB + i];
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
