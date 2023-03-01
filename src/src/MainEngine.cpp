#include "MainEngine.h"

#include <glad/glad.h>

#include <imgui.h>
#include <imgui_impl/imgui_impl_glfw.h>
#include <imgui_impl/imgui_impl_opengl3.h>

#include <stb_image.h>

#include "LoggingMacros.h"
#include "Model.h"
#include "Camera.h"
#include "Transform.h"
#include "Nodes/ModelNode.h"
#include "MouseHandler.h"
#include "Lights.h"
#include "Gizmos/Gizmo.h"
#include "Skybox.h"

#include "effolkronium/random.hpp"
#include "Nodes/FreeCameraNode.h"

using Random = effolkronium::random_static;

int32_t MainEngine::Init()
{
    glfwSetErrorCallback(MainEngine::GLFWErrorCallback);
    if (!glfwInit())
        return 1;

    const char* GLSLVersion = "#version 430";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

    glfwWindowHint(GLFW_SAMPLES, 4);

    if (InitializeWindow() != 0)
        return 1;

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    {
        SPDLOG_ERROR("Failed to initialize GLAD!");
        return 1;
    }
    SPDLOG_DEBUG("Successfully initialized OpenGL loader!");


    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetCursorPosCallback(window, MouseHandler::MouseCallback);

    InitializeImGui(GLSLVersion);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    Gizmo::Initialize();

    return 0;
}

int32_t MainEngine::InitializeWindow()
{
    window = glfwCreateWindow(640, 480, "Housing Estate", nullptr, nullptr);
    if (window == nullptr)
    {
        SPDLOG_ERROR("Failed to create OpenGL Window");
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(false);
    return 0;
}

void MainEngine::GLFWErrorCallback(int error, const char* description)
{
    SPDLOG_ERROR("GLFW error {}: {}", error, description);
}

int32_t MainEngine::MainLoop()
{
    auto startProgramTimePoint = std::chrono::high_resolution_clock::now();
    float previousFrameSeconds = 0;

#ifdef DEBUG
    CheckGLErrors();
#endif

    while (!glfwWindowShouldClose(window))
    {
        // TimeCalculation
        std::chrono::duration<float> timeFromStart = std::chrono::high_resolution_clock::now() - startProgramTimePoint;
        float seconds = timeFromStart.count();
        float deltaSeconds = seconds - previousFrameSeconds;
        previousFrameSeconds = seconds;

        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int displayX, displayY;
        glfwMakeContextCurrent(window);
        glfwGetFramebufferSize(window, &displayX, &displayY);
        glViewport(0, 0, displayX, displayY);

        sceneRoot.Update(this, seconds, deltaSeconds);
        sceneRoot.CalculateWorldTransform();
        sceneRoot.Draw();

        renderer.Draw(this);
        sceneLight->DrawGizmos();

        if (skybox)
            skybox->Draw();

        UpdateWidget(deltaSeconds);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return 0;
}

void MainEngine::UpdateWidget(float deltaSeconds)
{
    ImGui::Begin("Hi");

    ImGui::Separator();

    ImGui::Text("Framerate: %.3f (%.1f FPS)", deltaSeconds, 1 / deltaSeconds);

    ImGui::Separator();

    ImGui::Text("Point Light");
    PointLight bulb = sceneLight->GetBulb();
    ImGui::ColorEdit4("Point Light Color", (float*)&bulb.color);
    ImGui::DragFloat3("Point Light Position", (float*)&bulb.position);
    ImGui::DragFloat("Point Light Linear", &bulb.linear);
    ImGui::DragFloat("Point Light Quadratic", &bulb.quadratic);
    sceneLight->SetBulb(bulb);

    static glm::vec4 backgroundColor;
    ImGui::Text("Background");
    ImGui::ColorEdit4("Color", (float*)&backgroundColor);
    glClearColor(backgroundColor.x, backgroundColor.y, backgroundColor.z, backgroundColor.w);


    ImGui::Separator();

    ImGui::End();
}

MainEngine::MainEngine() : sceneRoot()
{
}

void MainEngine::InitializeImGui(const char* glslVersion)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    // Setup style
    ImGui::StyleColorsDark();
}

MainEngine::~MainEngine()
{
    Stop();
}

void MainEngine::Stop()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (!window)
        return;

    glfwDestroyWindow(window);
    glfwTerminate();
}

void MainEngine::CheckGLErrors()
{
    GLenum error;

    while ((error = glGetError()) != GL_NO_ERROR)
        SPDLOG_ERROR("OpenGL error: {}", error);
}

void MainEngine::PrepareScene()
{
    auto camera = std::make_shared<FreeCameraNode>(this);
    sceneRoot.AddChild(camera);
    camera->GetLocalTransform()->SetPosition({0, 0, -20});
    camera->SetActive();

    auto modelShader = std::make_shared<ShaderWrapper>("res/shaders/instanced.vert", "res/shaders/textured_model.frag");

    auto tardisModel = std::make_shared<Model>("res/models/Tardis/tardis.obj", modelShader);
    auto tardisNode = std::make_shared<ModelNode>(tardisModel, &renderer);
    sceneRoot.AddChild(tardisNode);

    auto crysisModel = std::make_shared<Model>("res/models/nanosuit/nanosuit.obj", modelShader);
    auto crysisNode = std::make_shared<ModelNode>(crysisModel, &renderer);
    sceneRoot.AddChild(crysisNode);
    crysisNode->GetLocalTransform()->SetPosition({-10, -10, 0});
    crysisNode->GetLocalTransform()->SetRotation(glm::quat({0, glm::pi<float>(), 0}));

    sceneLight = std::make_shared<Lights>();
}

GLFWwindow* MainEngine::GetWindow() const {
    return window;
}

unsigned int MainEngine::GetSkyboxTextureId() {
    return skybox->GetTextureId();
}
