# FirstSoloProjd
emcc src/main.cpp src/tinygltf.cpp -o build-wasm/index.js \
  -I src/vendor \
  -I src \
  -s USE_WEBGL2=1 \
  -s FULL_ES3=1 \
  -s USE_GLFW=3 \
  -s ALLOW_MEMORY_GROWTH=1 \
  --preload-file src/res@/res \
  -O2

emcc src/main.cpp src/tinygltf.cpp -o build-wasm/index.js \
  -I src/vendor \
  -I src \
  -s USE_WEBGL2=1 \
  -s FULL_ES3=1 \
  -s USE_GLFW=3 \
  -s ALLOW_MEMORY_GROWTH=1 \
  --preload-file src/res@/res \
  -O2


emcc src/main.cpp src/tinygltf.cpp -o build-wasm/index.html \
  -I src/vendor \
  -I src \
  -s USE_WEBGL2=1 \
  -s FULL_ES3=1 \
  -s USE_GLFW=3 \
  -s ALLOW_MEMORY_GROWTH=1 \
  --preload-file src/res@/res \
  -O2


  emcc main.cpp \
  -I/path/to/assimp/include \
  -I/path/to/assimp/build_wasm/include \
  -L/path/to/assimp/build_wasm/lib \
  -lassimp \
  -s USE_ZLIB=1 \
  -o index.html

  emcc src/main.cpp src/tinygltf.cpp -o build-wasm/index.html \
  -I src/vendor \
  -I src \
  -s USE_WEBGL2=1 \
  -s FULL_ES3=1 \
  -s USE_GLFW=3 \
  -s ALLOW_MEMORY_GROWTH=1 \
  --preload-file src/res@/res \
  -O2
THIS ONE WORKED



emcc src/main.cpp src/tinygltf.cpp -o build-wasm/index.js \
  -I src/vendor \
  -I src \
  -I src/vendor/assimp/include \
  -L src/vendor/assimp/build_wasm/lib \
  -lassimp \
  -s USE_WEBGL2=1 \
  -s FULL_ES3=1 \
  -s USE_GLFW=3 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ASSERTIONS=1 \
  -fexceptions \
  --preload-file src/res@/res \
  -O2