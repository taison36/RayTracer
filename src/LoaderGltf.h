#pragma once
#include "object/Model.h"

class LoaderGltf {
public:
    [[nodiscard]] std::optional<Model> load(const std::string& path) const;
};
