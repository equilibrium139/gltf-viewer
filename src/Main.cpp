#include "tiny_gltf/tiny_gltf.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

using namespace tinygltf;

Model model;
TinyGLTF loader;
std::string err;
std::string warn;

int windowWidth = 800;
int windowHeight = 600;

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

    Scene& scene = model.scenes[model.defaultScene];
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

    Node& node = model.nodes[scene.nodes[0]];
    Mesh& mesh = model.meshes[node.mesh];
    Primitive& primitive = mesh.primitives[0];
    int positionAccessorIdx = primitive.attributes["POSITION"];
    Accessor& positionAccessor = model.accessors[positionAccessorIdx];
    BufferView& bufferView = model.bufferViews[positionAccessor.bufferView];
    Buffer& buffer = model.buffers[bufferView.buffer];

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, buffer.data.size(), buffer.data.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, GetNumComponentsInType(positionAccessor.type), positionAccessor.componentType, GL_TRUE, positionAccessor.ByteStride(bufferView), (const void*)(bufferView.byteOffset + positionAccessor.byteOffset));

    int indicesAccessorIdx = primitive.indices;
    bool hasIndices = indicesAccessorIdx >= 0;
    int countIndices = -1;
    int indicesType = -1;
    int indicesOffset = 0;
    GLuint IBO;
    if (hasIndices)
    {
        Accessor& indicesAccessor = model.accessors[indicesAccessorIdx];
        countIndices = indicesAccessor.count;
        indicesType = indicesAccessor.componentType;
        BufferView& bufferView = model.bufferViews[indicesAccessor.bufferView];
        Buffer& buffer = model.buffers[bufferView.buffer];
        indicesOffset += indicesAccessor.byteOffset + bufferView.byteOffset;
        glGenBuffers(1, &IBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, buffer.data.size(), buffer.data.data(), GL_STATIC_DRAW);
    }


    const char* vsSource =
    R"(#version 330 core
       layout(location = 0) in vec3 aPos;
       void main()
       {
            gl_Position = vec4(aPos, 1.0);
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

    while (!glfwWindowShouldClose(window))
    {
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