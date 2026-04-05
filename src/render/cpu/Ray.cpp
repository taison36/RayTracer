#include "Ray.h"
namespace rt::cpu {

Ray::Ray(const glm::vec3& origin, const glm::vec3& direction) 
        : origin(origin), direction(direction) {};

} //rt
