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

void FramebufferSizeCallback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
    windowWidth = width;
    windowHeight = height;
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

Scene* LoadScene(const std::string& modelName, std::unordered_map<std::string, Scene>& scenes)
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
    auto pair = scenes.emplace(std::piecewise_construct, std::forward_as_tuple(modelName), std::forward_as_tuple(model.scenes[0], model));
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

    int selectedModelIndex = 3;
    while (sampleModelNames[selectedModelIndex] != "BrainStem")
    {
        selectedModelIndex++;
    }
    Scene* selectedScene = LoadScene(sampleModelNames[selectedModelIndex], sampleModels);

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
                    auto sceneIter = sampleModels.find(sampleModelNames[selectedModelIndex]);
                    if (sceneIter != sampleModels.end())
                    {
                        selectedScene = &sceneIter->second;
                    }
                    else
                    {
                        selectedScene = LoadScene(sampleModelNames[selectedModelIndex], sampleModels);
                    }
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
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (selectedScene)
        {
            selectedScene->UpdateAndRender(input);
        }

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