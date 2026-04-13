#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "primitive.h"
#include "material/material.h"
#include "../math/vec3.h"
#include "../math/ray.h"
#include <memory>

class Triangle : public Primitive {
private:
    Point3 v0, v1, v2;  // Triangle vertices
    Vec3 edge1, edge2;  // Precomputed edges for fast intersection
    std::shared_ptr<Material> material;
    Vec3 normal;        // Precomputed normal

public:
    Triangle(const Point3& vertex0, const Point3& vertex1, const Point3& vertex2,
             std::shared_ptr<Material> mat)
        : v0(vertex0), v1(vertex1), v2(vertex2), material(mat) {
        // Precompute edges
        edge1 = v1 - v0;
        edge2 = v2 - v0;
        // Precompute normal (assuming CCW winding order)
        normal = cross(edge1, edge2).normalized();
    }

    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        // Möller-Trumbore intersection algorithm
        Vec3 h = cross(r.direction(), edge2);
        float a = dot(edge1, h);

        // Ray is parallel to triangle
        if (a > -1e-8f && a < 1e-8f) {
            return false;
        }

        float f = 1.0f / a;
        Vec3 s = r.origin() - v0;
        float u = f * dot(s, h);

        // Check if u is outside triangle bounds
        if (u < 0.0f || u > 1.0f) {
            return false;
        }

        Vec3 q = cross(s, edge1);
        float v = f * dot(r.direction(), q);

        // Check if v is outside triangle bounds or u + v > 1
        if (v < 0.0f || u + v > 1.0f) {
            return false;
        }

        // Compute t to find intersection point
        float t = f * dot(edge2, q);

        // Ray intersection
        if (t < t_min || t > t_max) {
            return false;
        }

        // Fill hit record
        rec.t = t;
        rec.p = r.at(t);
        rec.set_face_normal(r, normal);
        rec.mat = material;

        return true;
    }

    // Get triangle vertices
    Point3 vertex0() const { return v0; }
    Point3 vertex1() const { return v1; }
    Point3 vertex2() const { return v2; }
};

#endif // TRIANGLE_H