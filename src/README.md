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
