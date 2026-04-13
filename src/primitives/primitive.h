#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include "../../src/math/ray.h"
#include "../../src/math/vec3.h"
#include <memory>

// Forward declarations
class Material;

struct HitRecord {
    Point3 p;
    Vec3 normal;
    float t;
    bool front_face;
    std::shared_ptr<Material> mat;

    // Set the face normal based on ray direction
    inline void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class Primitive {
public:
    virtual ~Primitive() = default;

    // Check if ray hits this primitive
    // Returns true if hit, with hit record filled in if t_min < t < t_max
    virtual bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const = 0;
};

#endif // PRIMITIVE_H
