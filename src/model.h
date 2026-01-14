#ifndef MODEL_H
#define MODEL_H

#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"
#include "shader.h"
#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>

#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#endif
#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#endif
#include "TinyGLTF/tiny_gltf.h"

using namespace std;
#define MAX_BONE_INFLUENCE 4

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

class Model
{
    struct Vertex vertices;
    struct Texture tex;

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
    void loadModel(string const &path)
    {
        string ext = path.substr(path.find_last_of(".") + 1);

        cout << "[DEBUG] Loading: " << path << " (" << ext << ")" << endl;

        if (ext == "gltf" || ext == "glb")
        {
            loadGLTF(path);
        }
        else
        {

            loadOBJ(path);
        }
    }

    void loadOBJ(string const &path)
    {

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            cout << "CRITICAL ERROR: Could not open file: " << path << endl;
            return;
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size))
        {
            cout << "CRITICAL ERROR: Failed to read file: " << path << endl;
            return;
        }

        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFileFromMemory(
            buffer.data(),
            size,
            aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices,
            "obj");

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode *node, const aiScene *scene)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene)
    {
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector;

            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;

            if (mesh->HasNormals())
            {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }

            if (mesh->mTextureCoords[0])
            {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            }
            else
            {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }
            vertices.push_back(vertex);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }
        return Mesh(vertices, indices, textures);
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

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    string filename = string(path);
    if (!directory.empty())
        filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 4);

    if (data)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
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
        cout << "Texture failed to load: " << path << endl;
        stbi_image_free(data);
    }
    return textureID;
}
#endif