#pragma once
#include "../core/Scene.h"

namespace rt {
class SceneLoader {
public:
    [[nodiscard]] static  Scene loadScene(const std::string &path, const Camera& camera) ;
};

}//rt
