#pragma once
#include "glm/glm.hpp"

namespace rt::cpu {

struct Ray {
    glm::vec3 origin;    
    glm::vec3 direction;

    Ray(const glm::vec3& origin, const glm::vec3& direction);
};

} //rt::cpu
