#include "Ray.h"
namespace rt {

Ray::Ray(const glm::vec3& origin, const glm::vec3& direction) 
        : origin(origin), direction(direction) {};

} //rt
