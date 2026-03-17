#include "LoaderGltf.h"

#include <print>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"

static std::string GetFilePathExtension(const std::string &filePath) {
  if (filePath.find_last_of('.') != std::string::npos)
    return filePath.substr(filePath.find_last_of('.') + 1);
  return "";
}

std::optional<Model> LoaderGltf::load(const std::string &path) const {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;
  const std::string ext = GetFilePathExtension(path);

  bool ret = false;
  if (ext.compare("glb") == 0) {
    // binary glTF.
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.c_str());
  } else {
    // ascii/json glTF.
    ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.c_str());
  }

  if (!warn.empty()) {
    std::println("[WARN]glTF parse warning: {}", warn);
  }

  if (!err.empty()) {
    std::println("[ERROR] glTF parse error: {}", err);
  }
  if (!ret) {
    std::println("[ERROR] Failed to load glTF: {}", path);
    return std::nullopt;
  }

  return std::nullopt;
}
