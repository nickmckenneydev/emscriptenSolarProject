#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>
#include <GLES3/gl3.h>
#else
#include <glad/glad.h> // Desktop loader handles everything
#include <GLFW/glfw3.h>
#endif
#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>

// Project Header
#include "shader.h"
#include "camera.h"
#include "model.h"
#include "mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "stb_image.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <random>
GLFWwindow *window;
Camera camera(glm::vec3(0.0f, 3.0f, 33.0f));

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float lastX = 800.0f / 2.0f;
float lastY = 600.0f / 2.0f;
bool firstMouse = true;

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Shader *planetsShader = nullptr;
Model *modelObjectMercury = nullptr;
Model *sunGLTF = nullptr;
Model *backpack = nullptr;
Mesh *pointsBlob = nullptr;
bool doRotate = true;
unsigned int WindowDiffuseMap;
unsigned int WallDiffuseMap;
unsigned int InteriorWallTexture;
unsigned int FloorDiffuseMap;
unsigned int WallsVAO, planeOneVAO, planeTwoVAO, planeThreeVAO, planeFourVAO, spaceFabricPlaneVAO;
unsigned int VBO[8];

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);
unsigned int createInteriorWallTexture();
void genVertexAttribs(GLuint *VAO, float *verticesName, GLuint *VBO, int size);

void draw(Shader &shader, GLuint VAO, unsigned int DiffuseMap, unsigned int SpecularMap, int verticesCount, glm::vec3 localCenter);
// test
void main_loop()
{

    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(window);

    glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();

    planetsShader->use();
    planetsShader->setMat4("projection", projection);
    planetsShader->setMat4("view", view);
    planetsShader->setVec3("viewPos", camera.Position);

    // Global Render State
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_STENCIL_TEST);

    // Interior Walls
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);
    glDepthMask(GL_FALSE);
    glStencilMask(0x00);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    draw(*planetsShader, WallsVAO, InteriorWallTexture, InteriorWallTexture, 36, glm::vec3(0.0f, 0.0f, 0.0f));

    // // Windows
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Window 1

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    draw(*planetsShader, planeOneVAO, WindowDiffuseMap, WindowDiffuseMap, 6, glm::vec3(0.5f, 0.0f, 0.0f));

    // Window 2
    glStencilMask(0xFF);
    glStencilFunc(GL_NOTEQUAL, 0x2, 0xFF);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    draw(*planetsShader, planeTwoVAO, WindowDiffuseMap, WindowDiffuseMap, 6, glm::vec3(0.0f, 0.0f, 0.5f)); // Plane Three Window
    glStencilMask(0xFF);
    glStencilFunc(GL_NOTEQUAL, 0x3, 0xFF);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    // Window 3
    draw(*planetsShader, planeThreeVAO, WindowDiffuseMap, WindowDiffuseMap, 6, glm::vec3(-0.5f, 0.0f, 0.0f)); // Plane Four Window
    glStencilMask(0xFF);
    glStencilFunc(GL_NOTEQUAL, 0x4, 0xFF);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    // Window 4
    draw(*planetsShader, planeFourVAO, WindowDiffuseMap, WindowDiffuseMap, 6, glm::vec3(0.0f, 0.0f, -0.5f)); // Exterior Walls
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 0, 0xFF);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    draw(*planetsShader, spaceFabricPlaneVAO, FloorDiffuseMap, FloorDiffuseMap, 6, glm::vec3(0.0f, -0.5f, 0.0f));
    draw(*planetsShader, WallsVAO, WallDiffuseMap, WindowDiffuseMap, 36, glm::vec3(0.0f, 0.0f, 0.0f));
    //-----------------------------------------------------------------------//
    glDisable(GL_BLEND);
    // Sun Model
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(0.2f));
    planetsShader->setMat4("model", model);
    sunGLTF->Draw(*planetsShader);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Mercury Model
    model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(3.0f));
    model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0, 1.0, 0.0));
    model = glm::translate(model, glm::vec3(2.5f, 0.0f, 0.0f));
    model = glm::rotate(model, (float)glfwGetTime() * 7.5f, glm::vec3(0.0, 1.0, 0.0));
    planetsShader->setMat4("model", model);
    modelObjectMercury->Draw(*planetsShader);
    //-----------------------------------------------------------------------//
    // Backpack Model
    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 2, 0xFF);
    model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(1.2f));
    planetsShader->setMat4("model", model);
    backpack->Draw(*planetsShader);
    // Pixels!!!!
    model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(3.0f));
    model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0, 1.0, 0.0));
    model = glm::translate(model, glm::vec3(2.5f, 0.0f, 0.0f));
    model = glm::rotate(model, (float)glfwGetTime() * 7.5f, glm::vec3(0.0, 1.0, 0.0));
    planetsShader->setMat4("model", model);
    pointsBlob->Draw(*planetsShader);
    //-----------------------------------------------------------------------//
    // Backpack Model
    glm::vec3 pointLightColors[] = {
        glm::vec3(0.0f, 0.0f, 0.0f)};
    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 4, 0xFF);
    model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(1.2f));
    planetsShader->setMat4("model", model);
    backpack->Draw(*planetsShader);
    // Pixels!!!!
    model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(3.0f));
    model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0, 1.0, 0.0));
    model = glm::translate(model, glm::vec3(2.5f, 0.0f, 0.0f));
    model = glm::rotate(model, (float)glfwGetTime() * 7.5f, glm::vec3(0.0, 1.0, 0.0));
    planetsShader->setMat4("model", model);
    pointsBlob->Draw(*planetsShader);
    //-----------------------------------------------------------------------//
    // Backpack Model
    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 3, 0xFF);
    model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(1.2f));
    planetsShader->setMat4("model", model);
    backpack->Draw(*planetsShader);
    // Reset State
    glStencilMask(0xFF);

    glfwSwapBuffers(window);
    glfwPollEvents();
}

int main()
{
    if (!glfwInit())
        return -1;

    // Desktop OpenGL Configuration (matches your shader version)
#ifdef __EMSCRIPTEN__
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Desktop OpenGL Solar Project", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // --- CRITICAL FOR NATIVE ---
#ifndef __EMSCRIPTEN__
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
#endif
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    stbi_set_flip_vertically_on_load(true);

    // InteriorWallTexture = createInteriorWallTexture();
    InteriorWallTexture = loadTexture("res/textures/purple.jpeg");
    planetsShader = new Shader("res/shaders/planets.vs", "res/shaders/planets.fs");

    modelObjectMercury = new Model("res/models/mercury/Mercury.obj");
    modelObjectMercury->SetDiffuseTexture("res/models/mercury/diffuse.png");

    backpack = new Model("res/models/backpack/backpack.obj");
    backpack->SetDiffuseTexture("res/models/backpack/diffuse.jpg");

    sunGLTF = new Model("res/models/sun/scene.gltf");
    sunGLTF->SetDiffuseTexture("res/models/sun/textures/material_1_baseColor.png");

    WindowDiffuseMap = loadTexture("res/textures/window.png");
    WallDiffuseMap = loadTexture("res/textures/wall.jpg");
    FloorDiffuseMap = loadTexture("res/textures/container.jpg");
    std::random_device randomDevice;
    std::mt19937 gen(randomDevice());
    std::uniform_real_distribution<float> dis(-50.0f, 50.0f);

    vector<Vertex> PointVertices;
    vector<unsigned int> indices;
    vector<Texture> textures;

    float pixelSize = 0.05f;

    for (unsigned int i = 0; i < 100000; i++)
    {
        glm::vec3 center(dis(gen), dis(gen), dis(gen));

        glm::vec3 positions[4] = {
            center + glm::vec3(-pixelSize, -pixelSize, 0.0f),
            center + glm::vec3(pixelSize, -pixelSize, 0.0f),
            center + glm::vec3(pixelSize, pixelSize, 0.0f),
            center + glm::vec3(-pixelSize, pixelSize, 0.0f)};

        glm::vec2 uvs[4] = {
            glm::vec2(0.0f, 0.0f),
            glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(0.0f, 1.0f)};

        unsigned int startIndex = PointVertices.size();
        for (int j = 0; j < 4; j++)
        {
            Vertex v;
            v.Position = positions[j];
            v.Normal = glm::vec3(0.0f, 0.0f, 1.0f);
            v.TexCoords = uvs[j];

            PointVertices.push_back(v);
        }

        // 1 QUAD
        indices.push_back(startIndex + 0);
        indices.push_back(startIndex + 1);
        indices.push_back(startIndex + 2);
        indices.push_back(startIndex + 0);
        indices.push_back(startIndex + 2);
        indices.push_back(startIndex + 3);
    }

    pointsBlob = new Mesh(PointVertices, indices, textures);

    float vertices[] = {
        // Back face (z = -0.5f)
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,
        // Front face (z = 0.5f)
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        // Left face (x = -0.5f)
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        // Right face (x = 0.5f)
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        // Bottom face (y = -0.5f)
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        // Top face (y = 0.5f)
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};

    float planeOneVerticies[] = {
        0.5f, 0.3f, -0.3f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // Top Right
        0.5f, -0.3f, 0.3f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // Bottom Left
        0.5f, -0.3f, -0.3f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // Bottom Right
        0.5f, 0.3f, -0.3f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
        0.5f, 0.3f, 0.3f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.3f, 0.3f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Plane Two (Front Face - z = 0.5)
    float planeTwoVerticies[] = {
        0.3f, 0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // Top Right
        -0.3f, -0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Bottom Left
        0.3f, -0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // Bottom Right
        0.3f, 0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        -0.3f, 0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        -0.3f, -0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};

    // Plane Three (Left Face - x = -0.5)
    float planeThreeVerticies[] = {
        -0.5f, 0.3f, 0.3f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,   // Top Right
        -0.5f, -0.3f, -0.3f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // Bottom Left
        -0.5f, -0.3f, 0.3f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,  // Bottom Right
        -0.5f, 0.3f, 0.3f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, 0.3f, -0.3f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.3f, -0.3f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Plane Four (Back Face - z = -0.5)
    float planeFourVerticies[] = {
        0.3f, 0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // Top Right
        -0.3f, -0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // Bottom Left
        -0.3f, 0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,  // Top Left
        0.3f, 0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
        0.3f, -0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, // Bottom Right
        -0.3f, -0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f};
    float spaceFabricVertices[] = {
        15.0f, -0.5f, 15.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f,
        -15.0f, -0.5f, 15.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -15.0f, -0.5f, -15.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f,

        15.0f, -0.5f, 15.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f,
        -15.0f, -0.5f, -15.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f,
        15.0f, -0.5f, -15.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f};
    genVertexAttribs(&WallsVAO, vertices, &VBO[0], sizeof(vertices));
    genVertexAttribs(&planeOneVAO, planeOneVerticies, &VBO[1], sizeof(planeOneVerticies));
    genVertexAttribs(&planeTwoVAO, planeTwoVerticies, &VBO[2], sizeof(planeTwoVerticies));
    genVertexAttribs(&planeThreeVAO, planeThreeVerticies, &VBO[3], sizeof(planeThreeVerticies));
    genVertexAttribs(&planeFourVAO, planeFourVerticies, &VBO[4], sizeof(planeFourVerticies));
    genVertexAttribs(&spaceFabricPlaneVAO, spaceFabricVertices, &VBO[5], sizeof(spaceFabricVertices));

// --- The Main Loop Swap ---
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    while (!glfwWindowShouldClose(window))
    {
        main_loop();
    }
#endif

    glfwTerminate();
    return 0;
}
void genVertexAttribs(GLuint *VAO, float *verticesName, GLuint *VBO, int size)
{
    glGenVertexArrays(1, VAO);
    glGenBuffers(1, VBO);
    glBindVertexArray(*VAO);
    glBindBuffer(GL_ARRAY_BUFFER, *VBO);
    glBufferData(GL_ARRAY_BUFFER, size, verticesName, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
}

void draw(Shader &shader, GLuint VAO, unsigned int DiffuseMap, unsigned int SpecularMap, int verticesCount, glm::vec3 localCenter)
{
    shader.use();
    glBindVertexArray(VAO);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, DiffuseMap);

    shader.setInt("texture1", 0);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(10.0f));
    // if (doRotate)
    //     model = glm::rotate(model, (float)glfwGetTime() * 1.0f, glm::vec3(0.0, 1.0, 0.0));
    shader.setMat4("model", model);

    glDrawArrays(GL_TRIANGLES, 0, verticesCount);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

// unsigned int createInteriorWallTexture()
// {
//     unsigned int textureID;
//     glGenTextures(1, &textureID);
//     glBindTexture(GL_TEXTURE_2D, textureID);
//     unsigned char white[] = {255, 255, 255, 100};
//     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//     return textureID;
// }

unsigned int loadTexture(char const *path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;

    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 4);

    if (data)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        const char *window = "res/textures/window.png";
        if (!strcmp(path, window))
        {
            cout << path << "is equal to" << window << endl;
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        else
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            cout << path << "FAILED" << window << endl;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}