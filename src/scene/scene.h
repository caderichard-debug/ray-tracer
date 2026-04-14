#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <memory>
#include "../primitives/primitive.h"
#include "../math/ray.h"
#include "light.h"

class Scene {
public:
    std::vector<std::shared_ptr<Primitive>> objects;
    std::vector<Light> lights;
    Color ambient_light; // Ambient illumination

    Scene() : ambient_light(0.1f, 0.1f, 0.1f) {}

    void add_object(std::shared_ptr<Primitive> obj) {
        objects.push_back(obj);
    }

    void add_light(const Light& light) {
        lights.push_back(light);
    }

    // Check if ray hits any object in the scene
    // Returns the closest hit (smallest t)
    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const {
        bool hit_anything = false;
        float closest_so_far = t_max;

        for (const auto& object : objects) {
            HitRecord temp_rec;
            if (object->hit(r, t_min, closest_so_far, temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;  // Copy only when we have a closer hit
            }
        }

        return hit_anything;
    }

    // Check if any object blocks the shadow ray
    // Returns true if shadow ray is blocked
    bool is_shadowed(const Ray& shadow_ray, float t_max) const {
        // Use local HitRecord to avoid allocations
        HitRecord shadow_rec;
        const float t_min = 0.001f;  // Avoid self-intersection

        for (const auto& object : objects) {
            if (object->hit(shadow_ray, t_min, t_max, shadow_rec)) {
                return true;  // Early exit on first hit
            }
        }
        return false;
    }
};

#endif // SCENE_H
