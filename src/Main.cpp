#include "tiny_gltf/tiny_gltf.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/detail/type_quat.hpp>
#include <glm/ext/matrix_transform.hpp>
#include "Camera.h"
#include "Shader.h"
#include <iostream>
#include <span>
#include <utility>

tinygltf::Model model;
tinygltf::TinyGLTF loader;
std::string err;
std::string warn;

int windowWidth = 800;
int windowHeight = 600;

void FramebufferSizeCallback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
    windowWidth = width;
    windowHeight = height;
}

void ProcessInput(GLFWwindow* window, Camera& camera, float dt)
{
    static bool first_poll = true;
    static double prevMouseX, prevMouseY;

    if (first_poll)
    {
        glfwGetCursorPos(window, &prevMouseX, &prevMouseY);
        first_poll = false;
    }

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    double mouseDeltaX = mouseX - prevMouseX;
    double mouseDeltaY = -(mouseY - prevMouseY);

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        camera.ProcessMouseMovement((float)mouseDeltaX, (float)mouseDeltaY);
    }

    prevMouseX = mouseX;
    prevMouseY = mouseY;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(CAM_FORWARD, dt);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(CAM_LEFT, dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(CAM_BACKWARD, dt);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(CAM_RIGHT, dt);
}

glm::mat4 GetNodeTransform(const tinygltf::Node& node)
{
    glm::mat4 transform(1.0f);

    if (node.matrix.size() == 16)
    {
        transform[0][0] = node.matrix[0];
        transform[0][1] = node.matrix[1];
        transform[0][2] = node.matrix[2];
        transform[0][3] = node.matrix[3];
        transform[1][0] = node.matrix[4];
        transform[1][1] = node.matrix[5];
        transform[1][2] = node.matrix[6];
        transform[1][3] = node.matrix[7];
        transform[2][0] = node.matrix[8];
        transform[2][1] = node.matrix[9];
        transform[2][2] = node.matrix[10];
        transform[2][3] = node.matrix[11];
        transform[3][0] = node.matrix[12];
        transform[3][1] = node.matrix[13];
        transform[3][2] = node.matrix[14];
        transform[3][3] = node.matrix[15];
    }
    else 
    {
        if (node.translation.size() == 3)
        {
            glm::vec3 translation(node.translation[0], node.translation[1], node.translation[2]);
            transform = glm::translate(transform, translation);
        }
        if (node.rotation.size() == 4)
        {
            glm::quat rotation(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            transform = transform * glm::mat4(rotation);
        }
        if (node.scale.size() == 3)
        {
            glm::vec3 scale(node.scale[0], node.scale[1], node.scale[2]);
            transform = glm::scale(transform, scale);
        }
    }

    return transform;
}

glm::mat4 GetNodeTransform(float time, const tinygltf::AnimationChannel& channel, const tinygltf::AnimationSampler& sampler, const tinygltf::Model& model)
{
    assert(channel.target_path == "translation" || channel.target_path == "scale" || channel.target_path == "rotation");

    const tinygltf::Accessor& keyframeAccessor = model.accessors[sampler.input];
    assert(keyframeAccessor.type == TINYGLTF_TYPE_SCALAR);
    const tinygltf::BufferView& keyframeBufferView = model.bufferViews[keyframeAccessor.bufferView];
    const tinygltf::Buffer& keyframeBuffer = model.buffers[keyframeBufferView.buffer];

    const tinygltf::Accessor& channelValuesAccessor = model.accessors[sampler.output];
    assert(channelValuesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
    const tinygltf::BufferView& channelValuesBufferView = model.bufferViews[channelValuesAccessor.bufferView];
    const tinygltf::Buffer& channelValuesBuffer = model.buffers[channelValuesBufferView.buffer];

    const float animDuration = keyframeAccessor.maxValues[0];
    const float clipTime = std::fmod(time, animDuration);
    
    int keyframeIndex = 0;

    int keyframeBufferOffset = keyframeAccessor.byteOffset + keyframeBufferView.byteOffset;
    std::span<float> keyframeTimes((float*)&keyframeBuffer.data[keyframeBufferOffset], keyframeAccessor.count);
    
    while (keyframeTimes[keyframeIndex] < clipTime)
    {
        keyframeIndex++;
    }

    assert(keyframeIndex > 0);

    float a = keyframeTimes[keyframeIndex - 1];
    float b = keyframeTimes[keyframeIndex];
    float t = (clipTime - a) / (b - a);

    int channelValuesBufferOffset = channelValuesAccessor.byteOffset + channelValuesBufferView.byteOffset;
    if (channel.target_path == "translation" || channel.target_path == "scale")
    {
        std::span<glm::vec3> channelValues((glm::vec3*)&channelValuesBuffer.data[channelValuesBufferOffset], channelValuesAccessor.count);
        const glm::vec3& a = channelValues[keyframeIndex - 1];
        const glm::vec3& b = channelValues[keyframeIndex];
        const glm::vec3 interpolated = t * a + (1.0f - t) * b;
        if (channel.target_path == "translation")
        {
            return glm::translate(glm::mat4(), interpolated);
        }
        else
        {
            return glm::scale(glm::mat4(), interpolated);
        }
    }
    else
    {
        std::span<glm::quat> channelValues((glm::quat*)&channelValuesBuffer.data[channelValuesBufferOffset], channelValuesAccessor.count);
        const glm::quat& a = channelValues[keyframeIndex - 1];
        const glm::quat& b = channelValues[keyframeIndex];
        const glm::quat interpolated = glm::slerp(a, b, t);
        return glm::mat4(1.0f) * glm::mat4(interpolated);
    }
}

std::pair<float, float> GetWeights(float time, const tinygltf::AnimationChannel& channel, const tinygltf::AnimationSampler& sampler, const tinygltf::Model)
{
    assert(channel.target_path == "weights");

    const tinygltf::Accessor& keyframeAccessor = model.accessors[sampler.input];
    assert(keyframeAccessor.type == TINYGLTF_TYPE_SCALAR);
    const tinygltf::BufferView& keyframeBufferView = model.bufferViews[keyframeAccessor.bufferView];
    const tinygltf::Buffer& keyframeBuffer = model.buffers[keyframeBufferView.buffer];

    const tinygltf::Accessor& weightsAccessor = model.accessors[sampler.output];
    assert(weightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
    const tinygltf::BufferView& weightsBufferView = model.bufferViews[weightsAccessor.bufferView];
    const tinygltf::Buffer& weightsBuffer = model.buffers[weightsBufferView.buffer];

    assert(weightsAccessor.count == keyframeAccessor.count * 2 && "Only 2 morph targets supported currently");

    int keyframeBufferOffset = keyframeAccessor.byteOffset + keyframeBufferView.byteOffset;
    std::span<float> keyframeTimes((float*)&keyframeBuffer.data[keyframeBufferOffset], keyframeAccessor.count);

    int weightsBufferOffset = weightsAccessor.byteOffset + weightsBufferView.byteOffset;
    std::span<float> weights((float*)&weightsBuffer.data[weightsBufferOffset], weightsAccessor.count);

    const float animDuration = keyframeAccessor.maxValues[0];
    const float clipTime = std::fmod(time, animDuration);

    int keyframeIndex = 0;
    while (keyframeTimes[keyframeIndex] < clipTime)
    {
        keyframeIndex++;
    }

    assert(keyframeIndex > 0);

    const float aTime = keyframeTimes[keyframeIndex - 1];
    const float bTime = keyframeTimes[keyframeIndex];
    const float t = (clipTime - aTime) / (bTime - aTime);

    float morphTarget1Weight;
    {
        const int index = keyframeIndex * 2;
        const float aWeight = weights[index - 2];
        const float bWeight = weights[index];
        morphTarget1Weight = aWeight * t + bWeight * (1.0f - t);
    }

    float morphTarget2Weight;
    {
        const int index = keyframeIndex * 2 + 1;
        const float aWeight = weights[index - 2];
        const float bWeight = weights[index];
        morphTarget2Weight = aWeight * t + bWeight * (1.0f - t);
    }

    return { morphTarget1Weight, morphTarget2Weight };
}

int main(int argc, char** argv)
{
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, argv[1]);
    //bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)

    if (!warn.empty()) {
        printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
        printf("Failed to parse glTF\n");
        return -1;
    }


    if (!glfwInit())
        return -1;

    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    tinygltf::Scene& scene = model.scenes[model.defaultScene];
	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, scene.name.c_str(), NULL, NULL);

    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		printf("Failed to initialize GLAD\n");
		return -1;
	}    

    glViewport(0, 0, windowWidth, windowHeight);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    assert(model.buffers.size() == 1);
    tinygltf::Node& node = model.nodes[scene.nodes[0]];
    tinygltf::Mesh& mesh = model.meshes[node.mesh];
    tinygltf::Primitive& primitive = mesh.primitives[0];
    int positionAccessorIdx = primitive.attributes["POSITION"];
    tinygltf::Accessor& positionAccessor = model.accessors[positionAccessorIdx];
    tinygltf::BufferView& bufferView = model.bufferViews[positionAccessor.bufferView];
    tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    
    glBufferData(GL_ARRAY_BUFFER, bufferView.byteLength, buffer.data.data() + bufferView.byteOffset, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, tinygltf::GetNumComponentsInType(positionAccessor.type), positionAccessor.componentType, GL_TRUE, positionAccessor.ByteStride(bufferView), (const void*)positionAccessor.byteOffset);
    bool hasMorphTargets = primitive.targets.size() > 0;
    if (hasMorphTargets)
    {
        GLuint morphTargetsVBO;
        glGenBuffers(1, &morphTargetsVBO);
        glBindBuffer(GL_ARRAY_BUFFER, morphTargetsVBO);

        assert(primitive.targets.size() == 2 && "Only 2 morph targets supported currently");
        // TODO:add support for other attributes like normals, tc etc
        tinygltf::Accessor& firstMorphTargetPositionAccessor = model.accessors[primitive.targets[0]["POSITION"]];
        tinygltf::Accessor& secondMorphTargetPositionAccessor = model.accessors[primitive.targets[1]["POSITION"]];
        tinygltf::BufferView& firstMorphTargetBV = model.bufferViews[firstMorphTargetPositionAccessor.bufferView];
        tinygltf::BufferView& secondMorphTargetBV = model.bufferViews[secondMorphTargetPositionAccessor.bufferView];
        assert(firstMorphTargetBV.byteLength == secondMorphTargetBV.byteLength);
        assert(firstMorphTargetBV.byteOffset < secondMorphTargetBV.byteOffset);

        // If morph targets are contiguous in buffer, send them to buffer data in one go
        if (firstMorphTargetBV.byteOffset + firstMorphTargetBV.byteLength == secondMorphTargetBV.byteOffset)
        {
            glBufferData(GL_ARRAY_BUFFER, 2 * firstMorphTargetBV.byteLength, buffer.data.data() + firstMorphTargetBV.byteOffset, GL_STATIC_DRAW);
        }
        else
        {
            glBufferData(GL_ARRAY_BUFFER, 2 * firstMorphTargetBV.byteLength, NULL, GL_STATIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, firstMorphTargetBV.byteLength, buffer.data.data() + firstMorphTargetBV.byteOffset);
            glBufferSubData(GL_ARRAY_BUFFER, firstMorphTargetBV.byteLength, secondMorphTargetBV.byteLength, buffer.data.data() + secondMorphTargetBV.byteOffset);
        }
        // Both morph targets should have the same values for these variables
        int numComponents = tinygltf::GetNumComponentsInType(firstMorphTargetPositionAccessor.type);
        int componentType = firstMorphTargetPositionAccessor.componentType;
        int stride = firstMorphTargetPositionAccessor.ByteStride(firstMorphTargetBV);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, numComponents, componentType, GL_TRUE, stride, 0);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, numComponents, componentType, GL_TRUE, stride, (const void*)firstMorphTargetBV.byteLength);
    }

    int indicesAccessorIdx = primitive.indices;
    bool hasIndices = indicesAccessorIdx >= 0;
    int countIndices = -1;
    int indicesType = -1;
    GLuint IBO;
    if (hasIndices)
    {
        tinygltf::Accessor& indicesAccessor = model.accessors[indicesAccessorIdx];
        countIndices = indicesAccessor.count;
        indicesType = indicesAccessor.componentType;
        tinygltf::BufferView& bufferView = model.bufferViews[indicesAccessor.bufferView];
        tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        glGenBuffers(1, &IBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, bufferView.byteLength, buffer.data.data() + bufferView.byteOffset, GL_STATIC_DRAW);
    }

    Shader defaultShader("Shaders/default.vert", "Shaders/default.frag");
    Shader morphShader("Shaders/morph.vert", "Shaders/default.frag");

    glm::mat4 transform = GetNodeTransform(node);
    bool hasAnimation = model.animations.size() == 1;

    float deltaTime = 0.0f;
    float previousFrameTime = 0.0f;
    float currentFrameTime = 0.0f;

    Camera camera;
    camera.position.z = 5.0f;

    while (!glfwWindowShouldClose(window))
    {
        // Update
        currentFrameTime = (float)glfwGetTime();
        deltaTime = currentFrameTime - previousFrameTime;
        previousFrameTime = currentFrameTime;

        ProcessInput(window, camera, deltaTime);

        if (hasAnimation)
        {
            if (!hasMorphTargets)
            {
                tinygltf::Animation& animation = model.animations[0];
                tinygltf::AnimationChannel& channel = animation.channels[0];
                tinygltf::AnimationSampler& sampler = animation.samplers[channel.sampler];
                transform = GetNodeTransform(glfwGetTime(), channel, sampler, model);
            }
            else
            {
                tinygltf::Animation& animation = model.animations[0];
                tinygltf::AnimationChannel& channel = animation.channels[0];
                tinygltf::AnimationSampler& sampler = animation.samplers[channel.sampler];
                std::pair<float, float> weights = GetWeights(glfwGetTime(), channel, sampler, model);
                morphShader.use();
                morphShader.SetFloat("morph1Weight", weights.first);
                morphShader.SetFloat("morph2Weight", weights.second);
            }
        }

        // Render
        glClear(GL_COLOR_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 proj = camera.GetProjectionMatrix((float)windowWidth / (float)windowHeight);
        glm::mat4 mvp = proj * view * transform;

        if (!hasMorphTargets)
        {
            defaultShader.use();
            defaultShader.SetMat4("mvp", glm::value_ptr(mvp));
        }
        else
        {
            morphShader.use();
            morphShader.SetMat4("mvp", glm::value_ptr(mvp));
        }

        if (!hasIndices)
        {
            glDrawArrays(primitive.mode, bufferView.byteOffset, positionAccessor.count);
        }
        else
        {
            glDrawElements(primitive.mode, countIndices, indicesType, 0);
        }

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glfwTerminate();
}