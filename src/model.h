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

// --- TINYGLTF HEADERS (For GLTF Files Only) ---
#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#endif
#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#endif
#include "TinyGLTF/tiny_gltf.h"

using namespace std;
#define MAX_BONE_INFLUENCE 4

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
    int m_BoneIDs[MAX_BONE_INFLUENCE];
    float m_Weights[MAX_BONE_INFLUENCE];
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
        for (unsigned int i = 0; i < textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            string name = textures[i].type;

            // FIX: UNIFORM NAMING
            const char *uniformName = nullptr;
            if (name == "texture_diffuse")
                uniformName = "material.diffuse";
            else if (name == "texture_specular")
                uniformName = "material.specular";

            if (uniformName)
                shader.setInt(uniformName, i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
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

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

// --- MODEL CLASS ---
class Model
{
public:
    vector<Texture> textures_loaded;
    vector<Mesh> meshes;
    string directory;
    bool gammaCorrection;

    Model(string const &path, bool gamma = false) : gammaCorrection(gamma)
    {
        directory = path.substr(0, path.find_last_of('/'));
        loadModel(path);
    }

    void Draw(Shader &shader)
    {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

    void SetDiffuseTexture(string path)
    {
        unsigned int id = TextureFromFile(path.c_str(), "", false);
        Texture tex;
        tex.id = id;
        tex.type = "texture_diffuse";
        tex.path = path;

        for (auto &mesh : meshes)
        {
            mesh.textures.clear();
            mesh.textures.push_back(tex);
        }
    }

private:
    // --- THE TRAFFIC COP ---
    void loadModel(string const &path)
    {
        string ext = path.substr(path.find_last_of(".") + 1);
        cout << "[DEBUG] Loading: " << path << " (" << ext << ")" << endl;

        if (ext == "gltf" || ext == "glb")
        {
            cout << "[DEBUG] Using TinyGLTF (JSON Loader)" << endl;
            loadGLTF(path);
        }
        else
        {
            cout << "[DEBUG] Using Manual OBJ Loader" << endl;
            loadOBJ(path);
        }
    }

    // --- 1. MANUAL OBJ LOADER (Stable for WebAssembly) ---
    void loadOBJ(string const &path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            cout << "CRITICAL ERROR: Could not open OBJ file: " << path << endl;
            return;
        }

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
                        {
                            vtIdx = stoi(vertexStr.substr(firstSlash + 1)) - 1;
                        }
                    }
                    else
                    {
                        vIdx = stoi(vertexStr) - 1;
                    }

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

    // --- 2. TINYGLTF LOADER (For GLTF) ---
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
        {
            cout << "TinyGLTF Error: " << err << endl;
            return;
        }

        for (const auto &gltfMesh : model.meshes)
        {
            for (const auto &primitive : gltfMesh.primitives)
            {
                processGLTFPrimitive(model, primitive);
            }
        }
    }

    void processGLTFPrimitive(tinygltf::Model &model, const tinygltf::Primitive &primitive)
    {
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

// --- ROBUST TEXTURE LOADER ---
unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    string filename = string(path);
    if (!directory.empty())
        filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;

    // FIX 1: FORCE 4 CHANNELS (Fixes slanted textures)
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 4);

    if (data)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        // FIX 2: ALIGNMENT (Fixes browser crashes on odd-sized images)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

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
        // Helpful debugging
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
#endif