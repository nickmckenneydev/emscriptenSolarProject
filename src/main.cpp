#include <emscripten.h>
#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>
#include <GLES3/gl3.h>

// Project Headers
#include "shader.h"
#include "camera.h"
#include "model.h"

// Vendor Headers (Available via -I src/vendor)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "stb_image.h" // Declaration only (Implementation in Texture.cpp)

#include <iostream>
#include <vector>

// ==========================================================================
// GLOBAL STATE
// ==========================================================================
GLFWwindow *window;
Camera camera(glm::vec3(15.0f, 5.0f, 13.0f));

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Mouse State
float lastX = 800.0f / 2.0f;
float lastY = 600.0f / 2.0f;
bool firstMouse = true;

// Screen Settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Global Objects
Shader *planetsShader = nullptr;
Model *modelObjectMercury = nullptr;
Model *sunGLTF = nullptr;
Model *backpack = nullptr;

// Texture IDs
unsigned int WindowDiffuseMap;
unsigned int WallDiffuseMap;
unsigned int WhiteTexture; // optimization: created once, used many times

// Geometry
unsigned int PlanetsVAO, rightplaneVAO, allOtherPlanesVAO;
unsigned int VBO[3]; // We use 3 buffers in the code below

// Lighting Data
glm::vec3 pointLightPositions[] = {
    glm::vec3(8.0f, 0.0f, 0.0f),
};
glm::vec3 pointLightColors[] = {
    glm::vec3(0.75f, 0.0f, 1.0f)};

// ==========================================================================
// FORWARD DECLARATIONS
// ==========================================================================
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);
unsigned int createWhiteTexture();

// Draw helper
void draw(Shader &shader, GLuint VAO, unsigned int DiffuseMap, int verticesCount, unsigned int SpecularMap = 0);

// ==========================================================================
// MAIN LOOP (Called every frame)
// ==========================================================================
void main_loop()
{
    // 1. Time Logic
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // 2. Input
    processInput(window);

    // 3. Clear Screen
    glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // 4. Matrix Setup
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();

    // 5. Shader Uniforms (Global)
    planetsShader->use();
    planetsShader->setMat4("projection", projection);
    planetsShader->setMat4("view", view);
    planetsShader->setVec3("viewPos", camera.Position);

    // Material
    planetsShader->setInt("material.specular", 1);
    planetsShader->setFloat("material.shininess", 3.0f);

    // Directional Light
    planetsShader->setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
    planetsShader->setVec3("dirLight.ambient", 0.3f, 0.24f, 0.14f);
    planetsShader->setVec3("dirLight.diffuse", 0.7f, 0.42f, 0.26f);
    planetsShader->setVec3("dirLight.specular", 0.5f, 0.5f, 0.5f);

    // Point Light
    planetsShader->setVec3("pointLights[0].position", pointLightPositions[0]);
    planetsShader->setVec3("pointLights[0].ambient", pointLightColors[0] * 0.1f);
    planetsShader->setVec3("pointLights[0].diffuse", pointLightColors[0]);
    planetsShader->setVec3("pointLights[0].specular", pointLightColors[0]);
    planetsShader->setFloat("pointLights[0].constant", 1.0f);
    planetsShader->setFloat("pointLights[0].linear", 0.09f);
    planetsShader->setFloat("pointLights[0].quadratic", 0.032f);

    // 6. Global Render State
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_STENCIL_TEST);

    // --- DRAW SCENE ---

    // A. Interior Walls (Use White Texture)
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);
    glDepthMask(GL_FALSE);
    glStencilMask(0x00);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    draw(*planetsShader, PlanetsVAO, WhiteTexture, 36);

    // B. Windows (Transparent-ish logic)
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDepthMask(GL_FALSE);

    // Right Window
    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    draw(*planetsShader, rightplaneVAO, WindowDiffuseMap, 6);

    // Other Windows
    glStencilMask(0xFF);
    glStencilFunc(GL_NOTEQUAL, 0x2, 0xFF);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    draw(*planetsShader, allOtherPlanesVAO, WindowDiffuseMap, 18);

    // C. Exterior Walls
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 0, 0xFF);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    draw(*planetsShader, PlanetsVAO, WallDiffuseMap, 36);

    // D. Sun Model
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(0.2f));
    planetsShader->setMat4("model", model);

    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    sunGLTF->Draw(*planetsShader);

    // E. Mercury Model
    model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(3.0f));
    model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0, 1.0, 0.0));
    model = glm::translate(model, glm::vec3(2.5f, 0.0f, 0.0f));
    model = glm::rotate(model, (float)glfwGetTime() * 5.5f, glm::vec3(0.0, 1.0, 0.0));
    planetsShader->setMat4("model", model);

    modelObjectMercury->Draw(*planetsShader);

    // F. Backpack Model
    model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(1.2f));
    planetsShader->setMat4("model", model);

    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 2, 0xFF);
    backpack->Draw(*planetsShader);

    // Reset State
    glStencilMask(0xFF);

    glfwSwapBuffers(window);
    glfwPollEvents();
}

// ==========================================================================
// MAIN
// ==========================================================================
int main()
{
    // Init GLFW
    if (!glfwInit())
    {
        std::cout << "Failed to init GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API); // <--- Critical for WebGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "WebAssembly OpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Flip textures for OpenGL
    stbi_set_flip_vertically_on_load(true);

    // ---------------------------------------------------------
    // ASSET LOADING
    // ---------------------------------------------------------

    // Create the white fallback texture once
    WhiteTexture = createWhiteTexture();

    // Load Shaders
    planetsShader = new Shader("res/shaders/planets.vs", "res/shaders/planets.fs");

    // Load Models
    // Ensure you have fixed model.h to use TinyGLTF or stripped Assimp!
    modelObjectMercury = new Model("res/models/mercury/Mercury 1K.obj");
    sunGLTF = new Model("res/models/sun/scene.gltf");
    backpack = new Model("res/models/backpack/backpack.obj");

    // Load Textures
    WindowDiffuseMap = loadTexture("res/textures/purple.jpeg");
    WallDiffuseMap = loadTexture("res/textures/wall.jpg");

    // ---------------------------------------------------------
    // BUFFERS SETUP
    // ---------------------------------------------------------

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

    float rightplane_vertices[] = {
        // positions          // normals           // texture coords

        // RIGHT FACE (x = 0.5f)
        0.5f, 0.3f, -0.3f, 1.0f, 0.0f, 0.0f, 0.625f, 0.5f,
        0.5f, -0.3f, 0.3f, 1.0f, 0.0f, 0.0f, 0.375f, 0.75f,
        0.5f, -0.3f, -0.3f, 1.0f, 0.0f, 0.0f, 0.375f, 0.5f,
        0.5f, 0.3f, -0.3f, 1.0f, 0.0f, 0.0f, 0.625f, 0.5f,
        0.5f, 0.3f, 0.3f, 1.0f, 0.0f, 0.0f, 0.625f, 0.75f,
        0.5f, -0.3f, 0.3f, 1.0f, 0.0f, 0.0f, 0.375f, 0.75f};
    float frontface_vertices[] = {
        // FRONT FACE (z = 0.5f)
        0.3f, 0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 0.625f, 0.75f,
        -0.3f, -0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 0.375f, 1.0f,
        0.3f, -0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 0.375f, 0.75f,
        0.3f, 0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 0.625f, 0.75f,
        -0.3f, 0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 0.625f, 1.0f,
        -0.3f, -0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 0.375f, 1.0f,

        // LEFT FACE (x = -0.5f)
        -0.5f, 0.3f, 0.3f, -1.0f, 0.0f, 0.0f, 0.625f, 0.5f,
        -0.5f, -0.3f, -0.3f, -1.0f, 0.0f, 0.0f, 0.375f, 0.75f,
        -0.5f, -0.3f, 0.3f, -1.0f, 0.0f, 0.0f, 0.375f, 0.5f,
        -0.5f, 0.3f, 0.3f, -1.0f, 0.0f, 0.0f, 0.625f, 0.5f,
        -0.5f, 0.3f, -0.3f, -1.0f, 0.0f, 0.0f, 0.625f, 0.75f,
        -0.5f, -0.3f, -0.3f, -1.0f, 0.0f, 0.0f, 0.375f, 0.75f,

        // BACK FACE (z = -0.5f)
        0.3f, 0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 0.625f, 0.75f,
        -0.3f, -0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 0.375f, 1.0f,
        -0.3f, 0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 0.375f, 0.75f,
        0.3f, 0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 0.625f, 0.75f,
        0.3f, -0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 0.625f, 1.0f,
        -0.3f, -0.3f, -0.5f, 0.0f, 0.0f, -1.0f, 0.375f, 1.0f};

    // VBO[0]: Planets
    glGenVertexArrays(1, &PlanetsVAO);
    glGenBuffers(1, &VBO[0]);
    glBindVertexArray(PlanetsVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // VBO[1]: Right Plane
    glGenVertexArrays(1, &rightplaneVAO);
    glGenBuffers(1, &VBO[1]);
    glBindVertexArray(rightplaneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rightplane_vertices), rightplane_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // VBO[2]: Other Planes
    glGenVertexArrays(1, &allOtherPlanesVAO);
    glGenBuffers(1, &VBO[2]);
    glBindVertexArray(allOtherPlanesVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(frontface_vertices), frontface_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Start Main Loop
    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}

// ==========================================================================
// HELPERS
// ==========================================================================

// Optimized Draw Call
void draw(Shader &shader, GLuint VAO, unsigned int DiffuseMap, int verticesCount, unsigned int SpecularMap)
{
    shader.use();
    glBindVertexArray(VAO);

    // Bind Diffuse
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, DiffuseMap);
    shader.setInt("material.diffuse", 0);

    // Bind Specular (Optional)
    if (SpecularMap != 0)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, SpecularMap);
        shader.setInt("material.specular", 1);
    }

    // Default Model Matrix (Rotates the object)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(10.0f));
    model = glm::rotate(model, (float)glfwGetTime() * 0.05f, glm::vec3(0.0, 1.0, 0.0));
    shader.setMat4("model", model);

    glDrawArrays(GL_TRIANGLES, 0, verticesCount);
}

// Create a simple 1x1 white texture for untextured objects
unsigned int createWhiteTexture()
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    unsigned char white[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return textureID;
}

unsigned int loadTexture(char const *path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;

    // FORCE 4 CHANNELS (RGBA) - Fixes color shifting issues
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 4);

    if (data)
    {
        GLenum format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);

        // CRITICAL FIX: Disable 4-byte alignment restriction
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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