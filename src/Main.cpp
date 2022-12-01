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

    glEnable(GL_DEPTH_TEST);

    assert(model.buffers.size() == 1);
    tinygltf::Node& node = model.nodes[scene.nodes[0]];
    tinygltf::Mesh& mesh = model.meshes[node.mesh];
    tinygltf::Primitive& primitive = mesh.primitives[0];
    int positionAccessorIdx = primitive.attributes["POSITION"];
    tinygltf::Accessor& positionAccessor = model.accessors[positionAccessorIdx];
    tinygltf::BufferView& positionBV = model.bufferViews[positionAccessor.bufferView];
    assert(positionBV.byteLength == 
        positionAccessor.count * tinygltf::GetComponentSizeInBytes(positionAccessor.componentType) * tinygltf::GetNumComponentsInType(positionAccessor.type) &&
        "Interleaved buffers not currently supported");
    assert(positionAccessor.byteOffset == 0);
    tinygltf::Buffer& buffer = model.buffers[positionBV.buffer];

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    GLuint positionsVBO;
    glGenBuffers(1, &positionsVBO);
    glBindBuffer(GL_ARRAY_BUFFER, positionsVBO);

    std::vector<glm::vec3> positions((glm::vec3*)(buffer.data.data() + positionBV.byteOffset), (glm::vec3*)(buffer.data.data() + positionBV.byteOffset) + positionAccessor.count);
    for (auto& v : positions) v *= glm::vec3(100.0f, 100.0f, 100.0f);

    glBufferData(GL_ARRAY_BUFFER, positionBV.byteLength, buffer.data.data() + positionBV.byteOffset, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, tinygltf::GetNumComponentsInType(positionAccessor.type), positionAccessor.componentType, GL_TRUE, positionAccessor.ByteStride(positionBV), (const void*)positionAccessor.byteOffset);

    auto normalAccessorIdxIter = primitive.attributes.find("NORMAL");
    bool hasNormals = normalAccessorIdxIter != primitive.attributes.end();
    
    if (hasNormals)
    {
        int normalAccessorIdx = normalAccessorIdxIter->second;
        tinygltf::Accessor& normalAccessor = model.accessors[normalAccessorIdx];
        tinygltf::BufferView& normalBV = model.bufferViews[normalAccessor.bufferView];
        assert(normalBV.byteLength ==
            normalAccessor.count * tinygltf::GetComponentSizeInBytes(normalAccessor.componentType) * tinygltf::GetNumComponentsInType(normalAccessor.type) &&
            "Interleaved buffers not currently supported");
        assert(normalAccessor.count == positionAccessor.count);
        assert(normalAccessor.byteOffset == 0);

        std::span<glm::vec3> normals((glm::vec3*)(buffer.data.data() + normalBV.byteOffset), normalAccessor.count);

        GLuint normalsVBO;
        glGenBuffers(1, &normalsVBO);
        glBindBuffer(GL_ARRAY_BUFFER, normalsVBO);
        glBufferData(GL_ARRAY_BUFFER, normalBV.byteLength, buffer.data.data() + normalBV.byteOffset, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, tinygltf::GetNumComponentsInType(normalAccessor.type), normalAccessor.componentType, GL_TRUE, normalAccessor.ByteStride(normalBV), (const void*)normalAccessor.byteOffset);
    }
        
    bool hasMorphTargets = primitive.targets.size() > 0;
    if (hasMorphTargets)
    {
        GLuint morphTargetsPositionsVBO;
        glGenBuffers(1, &morphTargetsPositionsVBO);
        glBindBuffer(GL_ARRAY_BUFFER, morphTargetsPositionsVBO);

        assert(primitive.targets.size() == 2 && "Only 2 morph targets supported currently");
        // TODO:add support for other attributes like texture coordinates
        tinygltf::Accessor& firstMorphTargetPositionAccessor = model.accessors[primitive.targets[0]["POSITION"]];
        tinygltf::Accessor& secondMorphTargetPositionAccessor = model.accessors[primitive.targets[1]["POSITION"]];
        tinygltf::BufferView& firstMorphTargetPositionBV = model.bufferViews[firstMorphTargetPositionAccessor.bufferView];
        tinygltf::BufferView& secondMorphTargetPositionBV = model.bufferViews[secondMorphTargetPositionAccessor.bufferView];
        assert(firstMorphTargetPositionBV.byteLength == secondMorphTargetPositionBV.byteLength);
        assert(firstMorphTargetPositionBV.byteOffset < secondMorphTargetPositionBV.byteOffset);

        // If morph targets are contiguous in buffer, send them to buffer data in one go
        if (firstMorphTargetPositionBV.byteOffset + firstMorphTargetPositionBV.byteLength == secondMorphTargetPositionBV.byteOffset)
        {
            glBufferData(GL_ARRAY_BUFFER, 2 * firstMorphTargetPositionBV.byteLength, buffer.data.data() + firstMorphTargetPositionBV.byteOffset, GL_STATIC_DRAW);
        }
        else
        {
            glBufferData(GL_ARRAY_BUFFER, 2 * firstMorphTargetPositionBV.byteLength, NULL, GL_STATIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, firstMorphTargetPositionBV.byteLength, buffer.data.data() + firstMorphTargetPositionBV.byteOffset);
            glBufferSubData(GL_ARRAY_BUFFER, firstMorphTargetPositionBV.byteLength, secondMorphTargetPositionBV.byteLength, buffer.data.data() + secondMorphTargetPositionBV.byteOffset);
        }
        // Both morph targets should have the same values for these variables
        int numComponents = tinygltf::GetNumComponentsInType(firstMorphTargetPositionAccessor.type);
        int componentType = firstMorphTargetPositionAccessor.componentType;
        int stride = firstMorphTargetPositionAccessor.ByteStride(firstMorphTargetPositionBV);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, numComponents, componentType, GL_TRUE, stride, 0);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, numComponents, componentType, GL_TRUE, stride, (const void*)firstMorphTargetPositionBV.byteLength);

        GLuint morphTargetsNormalsVBO;
        glGenBuffers(1, &morphTargetsNormalsVBO);
        glBindBuffer(GL_ARRAY_BUFFER, morphTargetsNormalsVBO);

        tinygltf::Accessor& firstMorphTargetNormalAccessor = model.accessors[primitive.targets[0]["NORMAL"]];
        tinygltf::Accessor& secondMorphTargetNormalAccessor = model.accessors[primitive.targets[1]["NORMAL"]];
        tinygltf::BufferView& firstMorphTargetNormalBV = model.bufferViews[firstMorphTargetNormalAccessor.bufferView];
        tinygltf::BufferView& secondMorphTargetNormalBV = model.bufferViews[secondMorphTargetNormalAccessor.bufferView];
        assert(firstMorphTargetNormalBV.byteLength == secondMorphTargetNormalBV.byteLength);
        assert(firstMorphTargetNormalBV.byteOffset < secondMorphTargetNormalBV.byteOffset);

        // If morph targets are contiguous in buffer, send them to buffer data in one go
        if (firstMorphTargetNormalBV.byteOffset + firstMorphTargetNormalBV.byteLength == secondMorphTargetNormalBV.byteOffset)
        {
            glBufferData(GL_ARRAY_BUFFER, 2 * firstMorphTargetNormalBV.byteLength, buffer.data.data() + firstMorphTargetNormalBV.byteOffset, GL_STATIC_DRAW);
        }
        else
        {
            glBufferData(GL_ARRAY_BUFFER, 2 * firstMorphTargetNormalBV.byteLength, NULL, GL_STATIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, firstMorphTargetNormalBV.byteLength, buffer.data.data() + firstMorphTargetNormalBV.byteOffset);
            glBufferSubData(GL_ARRAY_BUFFER, firstMorphTargetNormalBV.byteLength, secondMorphTargetNormalBV.byteLength, buffer.data.data() + secondMorphTargetNormalBV.byteOffset);
        }
        // Both morph targets should have the same values for these variables
        numComponents = tinygltf::GetNumComponentsInType(firstMorphTargetNormalAccessor.type);
        componentType = firstMorphTargetNormalAccessor.componentType;
        stride = firstMorphTargetNormalAccessor.ByteStride(firstMorphTargetNormalBV);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, numComponents, componentType, GL_TRUE, stride, 0);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, numComponents, componentType, GL_TRUE, stride, (const void*)firstMorphTargetNormalBV.byteLength);
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
        std::span<unsigned short> indices((unsigned short*)(buffer.data.data() + bufferView.byteOffset), indicesAccessor.count);
        glGenBuffers(1, &IBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, bufferView.byteLength, buffer.data.data() + bufferView.byteOffset, GL_STATIC_DRAW);
    }

    std::vector<std::string> defines;
    if (hasNormals) defines.push_back("HAS_NORMALS");
    if (hasMorphTargets) defines.push_back("HAS_MORPH_TARGETS");
    Shader defaultShader("Shaders/default.vert", "Shaders/default.frag", nullptr, {}, defines);
    //Shader morphShader("Shaders/morph.vert", "Shaders/default.frag");

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
                defaultShader.use();
                defaultShader.SetFloat("morph1Weight", weights.first);
                defaultShader.SetFloat("morph2Weight", weights.second);
            }
        }

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 modelView = view * transform;
        glm::mat4 projection = camera.GetProjectionMatrix((float)windowWidth / (float)windowHeight);

        defaultShader.use();
        defaultShader.SetMat4("modelView", glm::value_ptr(modelView));
        defaultShader.SetMat4("projection", glm::value_ptr(projection));
        defaultShader.SetVec3("pointLight.positionVS", glm::vec3(0.0f, 0.0f, 0.0f));
        defaultShader.SetVec3("pointLight.color", glm::vec3(0.5f, 0.5f, 0.5f));
        if (hasNormals)
        {
            glm::mat3 normalMatrixVS = glm::transpose(glm::inverse(glm::mat3(modelView)));
            defaultShader.SetMat3("normalMatrixVS", glm::value_ptr(normalMatrixVS));
        }

        if (!hasIndices)
        {
            glDrawArrays(primitive.mode, positionBV.byteOffset, positionAccessor.count);
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