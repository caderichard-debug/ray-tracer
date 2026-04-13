#ifndef RAY_H
#define RAY_H

#include "vec3.h"

class Ray {
public:
    Point3 orig;
    Vec3 dir;
    float tm; // Time motion (for motion blur, currently unused)

    // Constructors
    Ray() : tm(0) {}
    Ray(const Point3& origin, const Vec3& direction, float time = 0.0f)
        : orig(origin), dir(direction), tm(time) {}

    Point3 origin() const { return orig; }
    Vec3 direction() const { return dir; }
    float time() const { return tm; }

    // Get point at distance t along the ray
    Point3 at(float t) const {
        return orig + t * dir;
    }
};

#endif // RAY_H
