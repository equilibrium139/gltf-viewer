#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Input.h"
#include <memory>
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
    GLuint fullscreenQuadVAO,
    GLuint colorTexture,
    GLuint highlightTexture,
    GLuint depthStencilRBO)
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
    auto pair = scenes.emplace(std::piecewise_construct, std::forward_as_tuple(modelName), std::forward_as_tuple(model.scenes[0], model, windowWidth, windowHeight, fbo, fullscreenQuadVAO, colorTexture, highlightTexture, depthStencilRBO));
    return &pair.first->second;
}

int main(int argc, char** argv)
{
    if (!glfwInit())
        return -1;

    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
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

    glViewport(0, 0, windowWidth, windowHeight);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetKeyCallback(window, key_callback);

    glEnable(GL_DEPTH_TEST);

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

    int texW = windowWidth;
    int texH = windowHeight;

    GLuint colorTexture;
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, texW, texH, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    GLuint depthStencilRBO;
    glGenRenderbuffers(1, &depthStencilRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, texW, texH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBO);

    GLuint highlightTexture;
    glGenTextures(1, &highlightTexture);
    glBindTexture(GL_TEXTURE_2D, highlightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, texW, texH, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, highlightTexture, 0);

    static const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers); // this is framebuffer state so we only need to set it once

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    while (sampleModelNames[selectedModelIndex] != "InterpolationTest")
    {
        selectedModelIndex++;
    }
    Scene* selectedScene = LoadScene(sampleModelNames[selectedModelIndex], sampleModels, fbo, fullscreenQuadVAO, colorTexture, highlightTexture, depthStencilRBO);

    Shader highlightShader = Shader("Shaders/fullscreen.vert", "Shaders/highlight.frag");

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


        if (selectedScene)
        {
            ImGui::Begin("Lighting");   
            ImGui::SliderFloat("Exposure", &selectedScene->exposure, 0.0f, 10.0f);
            ImGui::End();
        }



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
            selectedScene = LoadScene(sampleModelNames[selectedModelIndex], sampleModels, fbo, fullscreenQuadVAO, colorTexture, highlightTexture, depthStencilRBO);
        }

        if (selectedScene)
        {
            selectedScene->UpdateAndRender(input);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth, windowHeight);
        glBindVertexArray(fullscreenQuadVAO);
        highlightShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, highlightTexture);
        highlightShader.SetInt("sceneColorsTexture", 0);
        highlightShader.SetInt("highlightTexture", 1);
        if (selectedScene) highlightShader.SetFloat("exposure", selectedScene->exposure);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
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