#ifndef MODEL_H
#define MODEL_H

#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"
#include "shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>

// TINYGLTF SETUP
#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#endif
#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#endif
#include "TinyGLTF/tiny_gltf.h"

using namespace std;

// --- SIMPLE MESH STRUCTS ---
struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct Texture
{
    unsigned int id;
    string type;
    string path;
};

// --- MESH CLASS ---
class Mesh
{
public:
    vector<Vertex> vertices;
    vector<unsigned int> indices;
    vector<Texture> textures;
    unsigned int VAO, VBO, EBO;

    Mesh(vector<Vertex> vertices, vector<unsigned int> indices, vector<Texture> textures)
    {
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;
        setupMesh();
    }

    void Draw(Shader &shader)
    {
        // Bind textures
        unsigned int diffuseNr = 1;
        for (unsigned int i = 0; i < textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);

            string name = textures[i].type;
            // FIX: Map "texture_diffuse" to your shader's "material.diffuse"
            if (name == "texture_diffuse")
            {
                glUniform1i(glGetUniformLocation(shader.ID, "material.diffuse"), i);
            }
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        // If no texture, bind white texture to slot 0 to prevent "Brick Backpack"
        if (textures.empty())
        {
            glActiveTexture(GL_TEXTURE0);
            // Ensure you create a 1x1 white texture in main and bind it here if needed,
            // but usually assigning a texture manually is better.
        }

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

private:
    void setupMesh()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, Normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, TexCoords));
        glBindVertexArray(0);
    }
};

// --- MODEL CLASS ---
class Model
{
public:
    vector<Texture> textures_loaded;
    vector<Mesh> meshes;
    string directory;

    Model(string const &path)
    {
        directory = path.substr(0, path.find_last_of('/'));
        loadModel(path);
    }

    void Draw(Shader &shader)
    {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

    // --- NEW FUNCTION: Manually set texture for all meshes ---
    void SetDiffuseTexture(string path)
    {
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        // Force 4 channels for WebGL
        unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrComponents, 4);
        if (data)
        {
            glBindTexture(GL_TEXTURE_2D, textureID);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Fix alignment
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
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
            return;
        }

        Texture tex;
        tex.id = textureID;
        tex.type = "texture_diffuse";
        tex.path = path;

        // Apply to all meshes
        for (auto &mesh : meshes)
        {
            mesh.textures.clear(); // Remove old (or empty) textures
            mesh.textures.push_back(tex);
        }
    }

private:
    void loadModel(string const &path)
    {
        string ext = path.substr(path.find_last_of(".") + 1);
        if (ext == "gltf" || ext == "glb")
            loadGLTF(path);
        else if (ext == "obj")
            loadOBJ(path);
        else
            cout << "Format not supported: " << ext << endl;
    }

    void loadOBJ(string const &path)
    {
        std::ifstream file(path);
        if (!file.is_open())
            return;

        vector<glm::vec3> temp_positions;
        vector<glm::vec3> temp_normals;
        vector<glm::vec2> temp_texcoords;
        vector<Vertex> vertices;
        vector<unsigned int> indices;

        string line;
        while (getline(file, line))
        {
            stringstream ss(line);
            string prefix;
            ss >> prefix;

            if (prefix == "v")
            {
                glm::vec3 temp;
                ss >> temp.x >> temp.y >> temp.z;
                temp_positions.push_back(temp);
            }
            else if (prefix == "vt")
            {
                glm::vec2 temp;
                ss >> temp.x >> temp.y;
                temp_texcoords.push_back(temp);
            }
            else if (prefix == "vn")
            {
                glm::vec3 temp;
                ss >> temp.x >> temp.y >> temp.z;
                temp_normals.push_back(temp);
            }
            else if (prefix == "f")
            {
                string vertexStr;
                for (int i = 0; i < 3; i++)
                {
                    ss >> vertexStr;
                    // Simple OBJ parsing logic
                    int vIdx = 0, vtIdx = 0, vnIdx = 0;
                    size_t firstSlash = vertexStr.find('/');
                    size_t secondSlash = vertexStr.find('/', firstSlash + 1);

                    if (firstSlash != string::npos)
                    {
                        vIdx = stoi(vertexStr.substr(0, firstSlash)) - 1;
                        if (secondSlash != string::npos)
                        {
                            string vtStr = vertexStr.substr(firstSlash + 1, secondSlash - firstSlash - 1);
                            if (!vtStr.empty())
                                vtIdx = stoi(vtStr) - 1;
                            vnIdx = stoi(vertexStr.substr(secondSlash + 1)) - 1;
                        }
                        else
                            vtIdx = stoi(vertexStr.substr(firstSlash + 1)) - 1;
                    }
                    else
                        vIdx = stoi(vertexStr) - 1;

                    Vertex vertex;
                    vertex.Position = temp_positions[vIdx];
                    if (vtIdx >= 0 && vtIdx < temp_texcoords.size())
                        vertex.TexCoords = temp_texcoords[vtIdx];
                    if (vnIdx >= 0 && vnIdx < temp_normals.size())
                        vertex.Normal = temp_normals[vnIdx];

                    vertices.push_back(vertex);
                    indices.push_back(indices.size());
                }
            }
        }
        vector<Texture> textures;
        meshes.push_back(Mesh(vertices, indices, textures));
    }

    void loadGLTF(string const &path)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        string err, warn;
        bool ret = false;
        if (path.find(".glb") != string::npos)
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        else
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);

        if (!ret)
            return;

        for (const auto &gltfMesh : model.meshes)
        {
            for (const auto &primitive : gltfMesh.primitives)
            {
                // ... (Same GLTF logic as previous, simplified for brevity)
                // In a real GLTF loader, we'd extract texture indices here.
                // For now, use SetDiffuseTexture() in main.cpp for reliability.
                processGLTFPrimitive(model, primitive);
            }
        }
    }

    void processGLTFPrimitive(tinygltf::Model &model, const tinygltf::Primitive &primitive)
    {
        //
        vector<Vertex> vertices;
        vector<unsigned int> indices;

        const float *posBuffer = nullptr;
        const float *normBuffer = nullptr;
        const float *texBuffer = nullptr;
        size_t count = 0;

        if (primitive.attributes.count("POSITION"))
        {
            const auto &accessor = model.accessors[primitive.attributes.at("POSITION")];
            const auto &bufferView = model.bufferViews[accessor.bufferView];
            posBuffer = reinterpret_cast<const float *>(&model.buffers[bufferView.buffer].data[bufferView.byteOffset + accessor.byteOffset]);
            count = accessor.count;
        }
        if (primitive.attributes.count("NORMAL"))
        {
            const auto &accessor = model.accessors[primitive.attributes.at("NORMAL")];
            const auto &bufferView = model.bufferViews[accessor.bufferView];
            normBuffer = reinterpret_cast<const float *>(&model.buffers[bufferView.buffer].data[bufferView.byteOffset + accessor.byteOffset]);
        }
        if (primitive.attributes.count("TEXCOORD_0"))
        {
            const auto &accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
            const auto &bufferView = model.bufferViews[accessor.bufferView];
            texBuffer = reinterpret_cast<const float *>(&model.buffers[bufferView.buffer].data[bufferView.byteOffset + accessor.byteOffset]);
        }

        for (size_t i = 0; i < count; i++)
        {
            Vertex v;
            if (posBuffer)
                v.Position = glm::vec3(posBuffer[i * 3], posBuffer[i * 3 + 1], posBuffer[i * 3 + 2]);
            if (normBuffer)
                v.Normal = glm::vec3(normBuffer[i * 3], normBuffer[i * 3 + 1], normBuffer[i * 3 + 2]);
            if (texBuffer)
                v.TexCoords = glm::vec2(texBuffer[i * 2], texBuffer[i * 2 + 1]);
            vertices.push_back(v);
        }

        if (primitive.indices >= 0)
        {
            const auto &accessor = model.accessors[primitive.indices];
            const auto &bufferView = model.bufferViews[accessor.bufferView];
            const auto &buffer = model.buffers[bufferView.buffer];
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                const unsigned short *buf = reinterpret_cast<const unsigned short *>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                for (size_t i = 0; i < accessor.count; i++)
                    indices.push_back(buf[i]);
            }
            else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                const unsigned int *buf = reinterpret_cast<const unsigned int *>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                for (size_t i = 0; i < accessor.count; i++)
                    indices.push_back(buf[i]);
            }
        }

        vector<Texture> textures;
        meshes.push_back(Mesh(vertices, indices, textures));
    }
};
#endif