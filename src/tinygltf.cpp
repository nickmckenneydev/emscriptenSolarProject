#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// Prevent TinyGLTF from using its own include logic
// (This avoids "file not found" errors when paths differ)
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE

// Manually include the STB headers FIRST so TinyGLTF can see the functions
#include "stb_image.h"                // Finds src/vendor/stb_image.h
#include "TinyGLTF/stb_image_write.h" // Finds src/vendor/TinyGLTF/stb_image_write.h

// Now include TinyGLTF
#include "TinyGLTF/tiny_gltf.h"