#ifndef MODEL_H
#define MODEL_H

#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Use STB Image from the vendor folder
#include "stb_image.h"

#include "mesh.h"
#include "shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

// TINYGLTF IMPLEMENTATION
// We define these to prevent multiple definitions if included elsewhere
#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#endif
#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#endif
#include "TinyGLTF/tiny_gltf.h"

using namespace std;

unsigned int TextureFromFile(const char *path, const string &directory);

class Model
{
public:
    // model data
    vector<Texture> textures_loaded; // stores all the textures loaded so far
    vector<Mesh> meshes;
    string directory;
    bool gammaCorrection;

    // constructor, expects a filepath to a 3D model.
    Model(string const &path, bool gamma = false) : gammaCorrection(gamma)
    {
        loadModel(path);
    }

    // draws the model, and thus all its meshes
    void Draw(Shader &shader)
    {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

private:
    // loads a model with supported file extensions
    void loadModel(string const &path)
    {
        // 1. Check Extension
        string ext = path.substr(path.find_last_of(".") + 1);

        if (ext == "gltf" || ext == "glb")
        {
            loadGLTF(path);
        }
        else if (ext == "obj")
        {
            loadOBJ(path);
        }
        else
        {
            cout << "ERROR::MODEL::FORMAT_NOT_SUPPORTED: " << ext << endl;
        }
    }

    // =========================================================
    // CUSTOM OBJ LOADER (Simple version for WebAssembly)
    // =========================================================
    void loadOBJ(string const &path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            cout << "ERROR::OBJ::FILE_NOT_FOUND: " << path << endl;
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
                // OBJ faces can be complex (v/vt/vn). We handle triangles only.
                // Format: f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
                string vertexStr;
                for (int i = 0; i < 3; i++)
                {
                    ss >> vertexStr;

                    // Parse indices (1-based in OBJ, need 0-based)
                    int vIdx = 0, vtIdx = 0, vnIdx = 0;

                    // Simple parser for slashes
                    size_t firstSlash = vertexStr.find('/');
                    size_t secondSlash = vertexStr.find('/', firstSlash + 1);

                    if (firstSlash != string::npos)
                    {
                        vIdx = stoi(vertexStr.substr(0, firstSlash)) - 1;

                        if (secondSlash != string::npos)
                        {
                            // format v/vt/vn or v//vn
                            string vtStr = vertexStr.substr(firstSlash + 1, secondSlash - firstSlash - 1);
                            if (!vtStr.empty())
                                vtIdx = stoi(vtStr) - 1;
                            vnIdx = stoi(vertexStr.substr(secondSlash + 1)) - 1;
                        }
                        else
                        {
                            // format v/vt
                            vtIdx = stoi(vertexStr.substr(firstSlash + 1)) - 1;
                        }
                    }
                    else
                    {
                        // format v
                        vIdx = stoi(vertexStr) - 1;
                    }

                    // Construct Vertex
                    Vertex vertex;
                    vertex.Position = temp_positions[vIdx];
                    if (vtIdx >= 0 && vtIdx < temp_texcoords.size())
                        vertex.TexCoords = temp_texcoords[vtIdx];
                    if (vnIdx >= 0 && vnIdx < temp_normals.size())
                        vertex.Normal = temp_normals[vnIdx];

                    vertices.push_back(vertex);
                    indices.push_back(indices.size()); // simple sequential index
                }
            }
        }

        // Create the mesh
        vector<Texture> textures; // For this simple OBJ loader, we skip auto-loading textures
        meshes.push_back(Mesh(vertices, indices, textures));
        cout << "Loaded OBJ: " << path << " (" << vertices.size() << " vertices)" << endl;
    }

    // =========================================================
    // TINYGLTF LOADER
    // =========================================================
    void loadGLTF(string const &path)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        string err;
        string warn;

        bool ret = false;
        if (path.find(".glb") != string::npos)
        {
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        }
        else
        {
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
        }

        if (!warn.empty())
            cout << "GLTF Warn: " << warn << endl;
        if (!err.empty())
            cout << "GLTF Err: " << err << endl;

        if (!ret)
        {
            cout << "Failed to parse GLTF: " << path << endl;
            return;
        }

        // Iterate through meshes
        for (const auto &gltfMesh : model.meshes)
        {
            for (const auto &primitive : gltfMesh.primitives)
            {
                processGLTFPrimitive(model, primitive);
            }
        }
        cout << "Loaded GLTF: " << path << endl;
    }

    void processGLTFPrimitive(tinygltf::Model &model, const tinygltf::Primitive &primitive)
    {
        vector<Vertex> vertices;
        vector<unsigned int> indices;

        // Get Accessors for Position, Normal, TexCoord
        const float *positionBuffer = nullptr;
        const float *normalBuffer = nullptr;
        const float *texCoordBuffer = nullptr;
        size_t vertexCount = 0;

        if (primitive.attributes.find("POSITION") != primitive.attributes.end())
        {
            const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
            positionBuffer = reinterpret_cast<const float *>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
            vertexCount = accessor.count;
        }

        if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
        {
            const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("NORMAL")];
            const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
            normalBuffer = reinterpret_cast<const float *>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        }

        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
        {
            const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
            const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
            texCoordBuffer = reinterpret_cast<const float *>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        }

        for (size_t i = 0; i < vertexCount; i++)
        {
            Vertex vertex;
            if (positionBuffer)
            {
                vertex.Position = glm::vec3(positionBuffer[i * 3], positionBuffer[i * 3 + 1], positionBuffer[i * 3 + 2]);
            }
            if (normalBuffer)
            {
                vertex.Normal = glm::vec3(normalBuffer[i * 3], normalBuffer[i * 3 + 1], normalBuffer[i * 3 + 2]);
            }
            if (texCoordBuffer)
            {
                vertex.TexCoords = glm::vec2(texCoordBuffer[i * 2], texCoordBuffer[i * 2 + 1]);
            }
            vertices.push_back(vertex);
        }

        // Indices
        if (primitive.indices >= 0)
        {
            const tinygltf::Accessor &accessor = model.accessors[primitive.indices];
            const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

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

        vector<Texture> textures; // Simple GLTF loader skipping auto-texture logic for brevity
        meshes.push_back(Mesh(vertices, indices, textures));
    }
};

unsigned int TextureFromFile(const char *path, const string &directory)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;

    // FORCE 4 CHANNELS
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 4);

    if (data)
    {
        GLenum format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);

        // CRITICAL FIX
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
#endif