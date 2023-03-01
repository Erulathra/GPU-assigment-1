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
#include "Lights.h"
#include "Gizmos/SphereGizmo.h"
#include "Skybox.h"

#include "effolkronium/random.hpp"
#include "Nodes/MotorcycleNode.h"
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
    glClearColor(0.f, 1.f, 1.f, 1.f);

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
    ImGui::Begin("MotorCycle B)");

    constinit static bool isMotorcycle{false};

    ImGui::Checkbox("Is motorcycle active", &isMotorcycle);
    Node* node = sceneRoot.GetChild([](Node* node) -> bool {
        if (dynamic_cast<MotorcycleNode*>(node))
            return true;
    });
    auto motorcycle = dynamic_cast<MotorcycleNode*>(node);
    motorcycle->SetIsActive(isMotorcycle);

    if (!isMotorcycle)
    {
        Node* node = sceneRoot.GetChild([](Node* node) -> bool {
            if (dynamic_cast<FreeCameraNode*>(node))
                return true;
        });
        auto freeCamera = dynamic_cast<FreeCameraNode*>(node);
        freeCamera->SetActive();
    }

    ImGui::Separator();

    ImGui::Text("Framerate: %.3f (%.1f FPS)", deltaSeconds, 1 / deltaSeconds);

    ImGui::Separator();
    
    constinit static glm::vec2 sunDirection{-30.f, 0.f};
    
    DirectionalLight sun = sceneLight->GetSun();
    ImGui::ColorEdit4("sun Color", (float*)&sun.color);
    ImGui::DragFloat2("sun Direction", (float*)&sunDirection);
    sun.direction = Lights::DirectionVector(glm::radians(sunDirection.x), glm::radians(sunDirection.y));

    ImGui::Text("Point Light");
    PointLight bulb = sceneLight->GetBulb();
    ImGui::ColorEdit4("Point Light Color", (float*)&bulb.color);
    ImGui::DragFloat3("Point Light Position", (float*)&bulb.position);
    ImGui::DragFloat("Point Light Linear", &bulb.linear);
    ImGui::DragFloat("Point Light Quadratic", &bulb.quadratic);

    constinit static glm::vec2 directionOne{-30.f, -30.f};

    ImGui::Text("Spotlight One");
    SpotLight spotLightOne = sceneLight->GetSpotLightOne();
    ImGui::ColorEdit4("SpotlightOne Color", (float*)&spotLightOne.color);
    ImGui::DragFloat3("SpotlightOne Position", (float*)&spotLightOne.position);
    ImGui::DragFloat2("SpotlightOne Direction", (float*)&directionOne);
    ImGui::DragFloat("SpotlightOne Linear", &spotLightOne.linear);
    ImGui::DragFloat("SpotlightOne Quadratic", &spotLightOne.quadratic);

    float cutOff = glm::degrees(spotLightOne.cutOff);
    float outerCutOff = glm::degrees(spotLightOne.outerCutOff);
    ImGui::DragFloat("SpotlightOne Cutoff", &cutOff);
    ImGui::DragFloat("SpotlightOne Outer Cutoff", &outerCutOff);

    spotLightOne.cutOff = glm::radians(cutOff);
    spotLightOne.outerCutOff = glm::radians(outerCutOff);
    spotLightOne.direction = Lights::DirectionVector(glm::radians(directionOne.x), glm::radians(directionOne.y));

    constinit static glm::vec2 directionTwo{-140.f, 40.f};

    ImGui::Text("Spotlight Two");
    SpotLight spotLightTwo = sceneLight->GetSpotLightTwo();
    ImGui::ColorEdit4("SpotlightTwo Color", (float*)&spotLightTwo.color);
    ImGui::DragFloat3("SpotlightTwo Position", (float*)&spotLightTwo.position);
    ImGui::DragFloat2("SpotlightTwo Direction", (float*)&directionTwo);
    ImGui::DragFloat("SpotlightTwo Linear", &spotLightTwo.linear);
    ImGui::DragFloat("SpotlightTwo Quadratic", &spotLightTwo.quadratic);

    cutOff = glm::degrees(spotLightTwo.cutOff);
    outerCutOff = glm::degrees(spotLightTwo.outerCutOff);
    ImGui::DragFloat("SpotlightTwo Cutoff", &cutOff);
    ImGui::DragFloat("SpotlightTwo Outer Cutoff", &outerCutOff);

    spotLightTwo.cutOff = glm::radians(cutOff);
    spotLightTwo.outerCutOff = glm::radians(outerCutOff);
    spotLightTwo.direction = Lights::DirectionVector(glm::radians(directionTwo.x), glm::radians(directionTwo.y));

    sceneLight->SetSun(sun);
    sceneLight->SetBulb(bulb);
    sceneLight->SetSpotLightOne(spotLightOne);
    sceneLight->SetSpotLightTwo(spotLightTwo);

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
    const int houses_rows = 50;
    auto camera = std::make_shared<FreeCameraNode>(this);
    sceneRoot.AddChild(camera);
    camera->SetActive();

    auto modelShader = std::make_shared<ShaderWrapper>("res/shaders/instanced.vert", "res/shaders/textured_model.frag");

    auto houseBaseModel = std::make_shared<Model>("res/models/Domek/Base.obj", modelShader);
    auto houseRoofModel = std::make_shared<Model>("res/models/Domek/Roof.obj", modelShader);
    auto planeModel = std::make_shared<Model>("res/models/Domek/Plane.obj", modelShader);

    auto grassNode = std::make_shared<ModelNode>(planeModel, &renderer);
    grassNode->GetLocalTransform()->SetScale(glm::vec3(houses_rows * 4.f));

    sceneRoot.AddChild(grassNode);

    for (int i = 0; i < houses_rows; ++i)
    {
        for (int j = 0; j < houses_rows; ++j)
        {
            auto houseBaseNode = std::make_shared<ModelNode>(houseBaseModel, &renderer);

            glm::vec3 housePosition(0.);
            housePosition.x = (i - houses_rows / 2.f) * 7.f;
            housePosition.z = (j - houses_rows / 2.f) * 7.f;
            housePosition.y = 1.f;

            houseBaseNode->GetLocalTransform()->SetPosition(housePosition);
            float rotationAngle = Random::get<float>(0, glm::pi<float>());

            houseBaseNode->GetLocalTransform()->SetRotation(
                    glm::rotate(glm::mat4(1.f), rotationAngle, glm::vec3(0.f, 1.f, 0.f)));

            auto homeRoofNode = std::make_shared<ModelNode>(houseRoofModel, &renderer);
            homeRoofNode->GetLocalTransform()->SetPosition(glm::vec3(0.f, 1.f, 0.f));
            houseBaseNode->AddChild(homeRoofNode);

            sceneRoot.AddChild(houseBaseNode);
        }
    }

    sceneLight = std::make_shared<Lights>();

    std::array<std::string, 6> cubemapPaths = {
        "res/textures/skybox/right.jpg",
        "res/textures/skybox/left.jpg",
        "res/textures/skybox/top.jpg",
        "res/textures/skybox/bottom.jpg",
        "res/textures/skybox/front.jpg",
        "res/textures/skybox/back.jpg"
    };

    auto skyboxShader = std::make_shared<ShaderWrapper>("res/shaders/skybox.vert", "res/shaders/skybox.frag");

    skybox = std::make_shared<Skybox>(cubemapPaths, skyboxShader);

    auto motorcycle = std::make_shared<MotorcycleNode>(this, &renderer);
    motorcycle->GetLocalTransform()->SetScale(glm::vec3(0.2f));
    sceneRoot.AddChild(motorcycle);
}

GLFWwindow* MainEngine::GetWindow() const {
    return window;
}

unsigned int MainEngine::GetSkyboxTextureId() {
    return skybox->GetTextureId();
}
