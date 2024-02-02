#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Input.h"
#include <memory>
#include "tiny_gltf/stb_image.h"
#include "tiny_gltf/tiny_gltf.h"
#include <unordered_map>
#include <filesystem>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "Scene.h"

int windowWidth = 1920;
int windowHeight = 1080;
int selectedModelIndex = 0;

void FramebufferSizeCallback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
    windowWidth = width;
    windowHeight = height;
}

void GLAPIENTRY
MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    if (type == GL_DEBUG_TYPE_ERROR)
    {
        fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_N && action == GLFW_PRESS)
    {
        selectedModelIndex++;
    }
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        selectedModelIndex--;
    }
}

void ProcessInput(GLFWwindow* window, Input& outInput, const ImGuiIO& io)
{
    static bool firstPoll = true;

    auto prevMouseX = outInput.mouseX;
    auto prevMouseY = outInput.mouseY;
    double currentMouseX, currentMouseY;
    glfwGetCursorPos(window, &currentMouseX, &currentMouseY);
    outInput.mouseX = (float)currentMouseX;
    outInput.mouseY = (float)currentMouseY;

    glfwGetWindowSize(window, &outInput.windowWidth, &outInput.windowHeight);

    if (firstPoll)
    {
        outInput.mouseDeltaX = 0.0;
        outInput.mouseDeltaY = 0.0;
        firstPoll = false;
    }
    else
    {
        if (!io.WantCaptureMouse)
        {
            outInput.mouseDeltaX = outInput.mouseX - prevMouseX;
            outInput.mouseDeltaY = prevMouseY - outInput.mouseY;
        }
        else
        {
            outInput.mouseDeltaX = 0.0;
            outInput.mouseDeltaY = 0.0;
        }
    }

    if (!io.WantCaptureMouse)
    {
        outInput.leftMousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    }
    else
    {
        outInput.leftMousePressed = false;
    }

    if (!io.WantCaptureKeyboard)
    {
        outInput.wPressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        outInput.sPressed = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        outInput.aPressed = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        outInput.dPressed = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    }
    else
    {
        outInput.wPressed = false;
        outInput.sPressed = false;
        outInput.aPressed = false;
        outInput.dPressed = false;
    }
}

Scene* LoadScene(const std::string& modelName, std::unordered_map<std::string, Scene>& scenes, GLuint fbo,
    GLuint fbW,
    GLuint fbH,
    GLuint fullscreenQuadVAO,
    GLuint colorTexture,
    GLuint highlightTexture,
    GLuint depthStencilRBO,
    GLuint lightsUBO,
    GLuint skyboxVAO,
    GLuint environmentMap,
    GLuint irradianceMap,
    GLuint prefilterMap,
    GLuint brdfLUT)
{
    std::string filepath = "C:\\dev\\gltf-models\\" + modelName + "\\glTF\\" + modelName + ".gltf";
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    //bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)

    if (!warn.empty()) {
        printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
        printf("Failed to parse glTF\n");
        return nullptr;
    }

    assert(model.scenes.size() == 1); // cba
    auto pair = scenes.emplace(std::piecewise_construct, std::forward_as_tuple(modelName), std::forward_as_tuple(model.scenes[0], model, fbW, fbH, fbo, fullscreenQuadVAO, colorTexture, highlightTexture, depthStencilRBO, lightsUBO, skyboxVAO, environmentMap, irradianceMap, prefilterMap, brdfLUT));
    return &pair.first->second;
}

struct CubemapFile
{
    static constexpr std::uint32_t correctMagicNumber = 'PMBC';
    struct Header
    {
        std::uint32_t magicNumber = correctMagicNumber;
        std::uint32_t mipmapLevels;
        std::uint32_t resolution;
        // TODO: specify pixel format
    };
    Header header;
    std::vector<std::uint8_t> pixels;
};

int BytesPerFaceBC6(std::uint32_t resolution, std::uint32_t mipmapLevels)
{
    int bytesNeeded = resolution * resolution;

    for (int i = 1; i < mipmapLevels; i++)
    {
        resolution = std::max(resolution / 2, 4u); // BC6 always works with 4x4 blocks
        bytesNeeded += resolution * resolution;
    }

    return bytesNeeded;
}

GLuint ReadCubemapFile(const char* path)
{
    std::ifstream file(path, std::ios::binary);

    CubemapFile data;
    file.read((char*)&data.header, sizeof(data.header));
    assert(data.header.magicNumber == CubemapFile::correctMagicNumber);
    const auto res = data.header.resolution;
    int bytesPerFace = BytesPerFaceBC6(res, data.header.mipmapLevels);
    data.pixels.resize(bytesPerFace * 6);
    file.read((char*)&data.pixels[0], data.pixels.size());

    GLuint cubemap;
    glGenTextures(1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int byteOffset = 0;
    for (unsigned int i = 0; i < 6; ++i)
    {
        int mipRes = res;

        for (unsigned int j = 0; j < data.header.mipmapLevels; j++)
        {
            int imageSize = mipRes >= 4 ? mipRes * mipRes : 4 * 4;
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, j, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, mipRes, mipRes, 0, imageSize, &data.pixels[byteOffset]);
            byteOffset += imageSize;
            mipRes /= 2;
        }
    }

    return cubemap;
}

int main(int argc, char** argv)
{
    if (!glfwInit())
        return -1;

    const char* glsl_version = "#version 430";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    //tinygltf::Scene& scene = model.scenes[model.defaultScene];
	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "glTF Viewer", NULL, NULL);

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

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    glViewport(0, 0, windowWidth, windowHeight);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetKeyCallback(window, key_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    std::unordered_map<std::string, Scene> sampleModels;
    std::vector<std::string> sampleModelNames;

    namespace fs = std::filesystem;

    const auto modelsDirectory = fs::path("C:/dev/gltf-models");
    assert(fs::is_directory(modelsDirectory));
    for (const auto& entry : fs::directory_iterator(modelsDirectory))
    {
        if (entry.is_directory())
        {
            sampleModelNames.emplace_back(entry.path().filename().string());
        }
    }


    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    Input input{};

    input.deltaTime = 0.0f;
    float previousFrameTime = 0.0f;
    float currentFrameTime = 0.0f;

    GLfloat vertices[] = {
        // Outline vertices
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    GLushort indices[] = {
        // Outline edges
        0, 1, 1, 2, 2, 3, 3, 0,  // Bottom face
        4, 5, 5, 6, 6, 7, 7, 4,  // Top face
        0, 4, 1, 5, 2, 6, 3, 7   // Vertical edges
    };

    GLuint boundingBoxVAO;
    glGenVertexArrays(1, &boundingBoxVAO);
    glBindVertexArray(boundingBoxVAO);

    GLuint boundingBoxVBO;
    glGenBuffers(1, &boundingBoxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, boundingBoxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);

    GLuint boundingBoxIBO;
    glGenBuffers(1, &boundingBoxIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, boundingBoxIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    GLfloat quadVertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };

    GLuint quadIndices[] = {
        0, 1, 2,
        2, 3, 0
    };

    GLuint fullscreenQuadVAO;
    glGenVertexArrays(1, &fullscreenQuadVAO);
    glBindVertexArray(fullscreenQuadVAO);

    GLuint quadVBO;
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);

    GLuint quadIBO;
    glGenBuffers(1, &quadIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, windowWidth, windowHeight);

    // All scenes always render to framebuffer with fbW * fbH resolution. Scene renderer doesn't worry about scene resizing (for now)
    const int fbW = windowWidth;
    const int fbH = windowHeight;

    GLuint colorTexture;
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, fbW, fbH, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    GLuint depthStencilRBO;
    glGenRenderbuffers(1, &depthStencilRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fbW, fbH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBO);

    GLuint highlightTexture;
    glGenTextures(1, &highlightTexture);
    glBindTexture(GL_TEXTURE_2D, highlightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, fbW, fbH, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, highlightTexture, 0);

    static const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers); // this is framebuffer state so we only need to set it once

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLuint lightsUBO;
    glGenBuffers(1, &lightsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
    glBufferData(GL_UNIFORM_BUFFER, Shader::maxPointLights * sizeof(PointLight) + Shader::maxSpotLights * sizeof(SpotLight) + Shader::maxDirLights * sizeof(DirectionalLight) + 3 * sizeof(int), NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, lightsUBO);    

    glm::vec3 cubeVertices[] = {
        {-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 1.0f},  // POSITIVE_X

        {-1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, -1.0f},    // NEGATIVE_X

        {-1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 1.0f},   // POSITIVE_Y // TODO: fix the rest (match uv with pos)

        {-1.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, -1.0f}, // NEGATIVE_Y

         {-1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 1.0f},   // POSITIVE_Z

        {-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, -1.0f}, // NEGATIVE_Z
    };

    GLuint cubeIndices[] = {
        0, 1, 2, 2, 3, 0, 
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    GLuint cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glBindVertexArray(cubeVAO);
    
    GLuint cubeVBO;
    glGenBuffers(1, &cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    
    GLuint cubeIBO;
    glGenBuffers(1, &cubeIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec3), 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec3), (const void*)(sizeof(glm::vec3)));

    GLuint environmentMap = ReadCubemapFile("envmap.cubemap");
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);

    GLuint captureFBO;
    glGenFramebuffers(1, &captureFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    GLuint irradianceMap;
    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0,
            GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    Shader convolutionShader = Shader("Shaders/equirectToCubemap.vert", "Shaders/convolute.frag");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
    convolutionShader.SetInt("environmentMap", 0);

    glViewport(0, 0, 32, 32);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)(i * 6 * sizeof(GLuint)));
    }

    GLuint prefilterMap;
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    Shader prefilterShader = Shader("Shaders/equirectToCubemap.vert", "Shaders/prefilter.frag");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
    prefilterShader.SetInt("environmentMap", 0);
    prefilterShader.SetFloat("environmentMapResolution", 2048); // TODO: don't hardcode this
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        // reisze framebuffer according to mip-level size.
        unsigned int mipWidth = 128 * std::pow(0.5, mip);
        unsigned int mipHeight = 128 * std::pow(0.5, mip);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        prefilterShader.SetFloat("roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)(i * 6 * sizeof(GLuint)));
        }
    }

    GLuint brdfLUT;
    glGenTextures(1, &brdfLUT);

    // pre-allocate enough memory for the LUT texture.
    glBindTexture(GL_TEXTURE_2D, brdfLUT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUT, 0);

    glViewport(0, 0, 512, 512);
    glBindVertexArray(fullscreenQuadVAO);
    Shader brdfShader = Shader("Shaders/fullscreen.vert", "Shaders/brdfLUT.frag");
    brdfShader.use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);

    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    while (sampleModelNames[selectedModelIndex] != "MetalRoughSpheres")
    {
        selectedModelIndex++;
    }
    Scene* selectedScene = LoadScene(sampleModelNames[selectedModelIndex], sampleModels, fbo, fbW, fbH, fullscreenQuadVAO, colorTexture, highlightTexture, depthStencilRBO, lightsUBO, skyboxVAO, environmentMap, irradianceMap, prefilterMap, brdfLUT);

    Shader postprocessShader = Shader("Shaders/fullscreen.vert", "Shaders/postprocess.frag");

    while (!glfwWindowShouldClose(window))
    {
        // Update
        currentFrameTime = (float)glfwGetTime();
        input.deltaTime = currentFrameTime - previousFrameTime;
        previousFrameTime = currentFrameTime;

        ProcessInput(window, input, io);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Model Select");

        const int num_models = (int)sampleModelNames.size();
        auto& model_combo_preview_value = sampleModelNames[selectedModelIndex];
        if (ImGui::BeginCombo("Model", model_combo_preview_value.c_str()))
        {
            for (int n = 0; n < num_models; n++)
            {
                const bool is_selected = (selectedModelIndex == n);
                if (ImGui::Selectable(sampleModelNames[n].c_str(), is_selected))
                {
                    selectedModelIndex = n;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::End();

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        

        auto sceneIter = sampleModels.find(sampleModelNames[selectedModelIndex]);
        if (sceneIter != sampleModels.end())
        {
            selectedScene = &sceneIter->second;
        }
        else
        {
            selectedScene = LoadScene(sampleModelNames[selectedModelIndex], sampleModels, fbo, fbW, fbH, fullscreenQuadVAO, colorTexture, highlightTexture, depthStencilRBO, lightsUBO, skyboxVAO, environmentMap, irradianceMap, prefilterMap, brdfLUT);
        }

        if (selectedScene)
        {
            selectedScene->UpdateAndRender(input);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, display_w, display_h);
        glBindVertexArray(fullscreenQuadVAO);
        postprocessShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, highlightTexture);
        postprocessShader.SetInt("sceneColorsTexture", 0);
        postprocessShader.SetInt("highlightTexture", 1);
        if (selectedScene) postprocessShader.SetFloat("exposure", selectedScene->exposure);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, fbW, fbH);
        static GLuint binaryImageClearValue = 0;
        static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearBufferfv(GL_COLOR, 0, &clear_color.x);

        glColorMaski(1, 0xFF, 0xFF, 0xFF, 0xFF); // ensure the texture used for highlighting can be cleared
        glClearBufferuiv(GL_COLOR, 1, &binaryImageClearValue);

        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}