#ifndef SPHERE_H
#define SPHERE_H

#include "primitive.h"
#include "../material/material.h"

class Sphere : public Primitive {
public:
    Point3 center;
    float radius;
    std::shared_ptr<Material> mat;

    // Constructors
    Sphere() {}
    Sphere(Point3 cen, float r, std::shared_ptr<Material> m)
        : center(cen), radius(r), mat(m) {}

    // Ray-sphere intersection using analytic solution
    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        Vec3 oc = r.origin() - center;
        float a = r.direction().length_squared();
        float half_b = dot(oc, r.direction());
        float c = oc.length_squared() - radius * radius;

        float discriminant = half_b * half_b - a * c;

        // No intersection if discriminant is negative
        if (discriminant < 0) {
            return false;
        }

        float sqrtd = std::sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range
        float root = (-half_b - sqrtd) / a;
        if (root < t_min || t_max < root) {
            root = (-half_b + sqrtd) / a;
            if (root < t_min || t_max < root) {
                return false;
            }
        }

        // Fill in the hit record
        rec.t = root;
        rec.p = r.at(rec.t);
        rec.mat = mat;

        // Calculate outward normal and set face normal
        Vec3 outward_normal = (rec.p - center) / radius;
        rec.set_face_normal(r, outward_normal);

        return true;
    }
};

#endif // SPHERE_H
