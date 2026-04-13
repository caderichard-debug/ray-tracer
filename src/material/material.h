#ifndef MATERIAL_H
#define MATERIAL_H

#include "../math/vec3.h"
#include "../math/ray.h"
#include "../primitives/primitive.h"

class Material {
public:
    Color albedo; // Base color/reflectance

    Material() : albedo(1, 1, 1) {}
    Material(const Color& a) : albedo(a) {}
    virtual ~Material() = default;

    // Calculate scattered ray and attenuation
    // Returns true if the ray is scattered, false if absorbed
    virtual bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const = 0;
};

class Lambertian : public Material {
public:
    Lambertian(const Color& a) : Material(a) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        (void)r_in; // Unused parameter
        // Scatter randomly in the hemisphere around the normal
        auto scatter_direction = rec.normal + Vec3::random();

        // Catch degenerate scatter direction
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;

        scattered = Ray(rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }
};

class Metal : public Material {
public:
    float fuzz; // Roughness - 0 is perfect mirror, higher is more fuzzy

    Metal(const Color& a, float f) : Material(a), fuzz(f < 1 ? f : 1) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        // Reflect the ray around the normal
        Vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
        scattered = Ray(rec.p, reflected + fuzz * Vec3::random(-1, 1));
        attenuation = albedo;
        return (dot(scattered.direction(), rec.normal) > 0);
    }
};

#endif // MATERIAL_H
