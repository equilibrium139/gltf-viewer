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
#include <iostream>
#include <span>

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

    std::cout << mouseX << '\n';
    std::cout << mouseY << '\n';

    double mouseDeltaX = mouseX - prevMouseX;
    double mouseDeltaY = -(mouseY - prevMouseY);

    //std::cout << mouseDeltaX << '\n';
    //std::cout << mouseDeltaY << '\n';

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
            glm::quat rotation(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
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
    if (channel.target_path == "rotation")
    {
        std::span<glm::quat> channelValues((glm::quat*)&channelValuesBuffer.data[channelValuesBufferOffset], channelValuesAccessor.count);
        const glm::quat& a = channelValues[keyframeIndex - 1];
        const glm::quat& b = channelValues[keyframeIndex];
        const glm::quat interpolated = glm::slerp(a, b, t);
        return glm::mat4(1.0f) * glm::mat4(interpolated);
    }

    assert(channel.target_path != "weights" && "Weights unsupported currently.");
    return glm::mat4();
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
    glBufferData(GL_ARRAY_BUFFER, buffer.data.size(), buffer.data.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, tinygltf::GetNumComponentsInType(positionAccessor.type), positionAccessor.componentType, GL_TRUE, positionAccessor.ByteStride(bufferView), (const void*)(bufferView.byteOffset + positionAccessor.byteOffset));

    int indicesAccessorIdx = primitive.indices;
    bool hasIndices = indicesAccessorIdx >= 0;
    int countIndices = -1;
    int indicesType = -1;
    int indicesOffset = 0;
    GLuint IBO;
    if (hasIndices)
    {
        tinygltf::Accessor& indicesAccessor = model.accessors[indicesAccessorIdx];
        countIndices = indicesAccessor.count;
        indicesType = indicesAccessor.componentType;
        tinygltf::BufferView& bufferView = model.bufferViews[indicesAccessor.bufferView];
        tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        indicesOffset += indicesAccessor.byteOffset + bufferView.byteOffset;
        glGenBuffers(1, &IBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, buffer.data.size(), buffer.data.data(), GL_STATIC_DRAW);
    }

    const char* vsSource =
    R"(#version 330 core
       layout(location = 0) in vec3 aPos;
       uniform mat4 mvp;
       void main()
       {
            gl_Position = mvp * vec4(aPos, 1.0);
       })";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSource, 0);
    glCompileShader(vs);

    const char* fsSource =
    R"(#version 330 core
        out vec4 color;
        void main()
        {
            color = vec4(1.0, 0.0, 0.0, 1.0);
        })";
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSource, 0);
    glCompileShader(fs);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glUseProgram(program);

    glm::mat4 transform = GetNodeTransform(node);
    GLuint mvpLocation = glGetUniformLocation(program, "mvp");
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
            tinygltf::Animation& animation = model.animations[0];
            tinygltf::AnimationChannel& channel = animation.channels[0];
            tinygltf::AnimationSampler& sampler = animation.samplers[channel.sampler];
            transform = GetNodeTransform(glfwGetTime(), channel, sampler, model);
        }

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 proj = camera.GetProjectionMatrix((float)windowWidth / (float)windowHeight);
        glm::mat4 mvp = proj * view * transform;
        glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));

        // Render
        glClear(GL_COLOR_BUFFER_BIT);

        if (!hasIndices)
        {
            glDrawArrays(primitive.mode, bufferView.byteOffset, positionAccessor.count);
        }
        else
        {
            glDrawElements(primitive.mode, countIndices, indicesType, (const void*)indicesOffset);
        }

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glfwTerminate();
}