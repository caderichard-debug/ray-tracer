#ifndef MATERIAL_H
#define MATERIAL_H

#include "../math/vec3.h"
#include "../math/ray.h"
#include "../primitives/primitive.h"
#include "../texture/texture.h"
#include <random>
#include <cmath>
#include <memory>

class Material {
public:
    Color albedo; // Base color/reflectance
    float specular_intensity; // Specular highlight strength (0.0 = matte, 1.0 = shiny)
    float shininess; // Specular exponent (higher = smaller, sharper highlights)

    Material() : albedo(1, 1, 1), specular_intensity(0.0f), shininess(1.0f) {}
    Material(const Color& a) : albedo(a), specular_intensity(0.0f), shininess(1.0f) {}
    Material(const Color& a, float spec, float shine) : albedo(a), specular_intensity(spec), shininess(shine) {}
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
    std::shared_ptr<Texture> albedo_texture;

    Lambertian(const Color& a) : Material(a, 0.0f, 1.0f) { // Very matte, no specular
        albedo_texture = std::make_shared<SolidColor>(a);
    }

    Lambertian(std::shared_ptr<Texture> tex) : Material(Color(1,1,1), 0.0f, 1.0f), albedo_texture(tex) {} // Matte

    bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        (void)r_in; // Unused parameter
        // Scatter randomly in the hemisphere around the normal
        auto scatter_direction = rec.normal + Vec3::random();

        // Catch degenerate scatter direction
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;

        scattered = Ray(rec.p, scatter_direction);

        // Use texture for albedo
        attenuation = albedo_texture->value(0, 0, rec.p);
        return true;
    }
};

class Metal : public Material {
public:
    float fuzz; // Roughness - 0 is perfect mirror, higher is more fuzzy

    Metal(const Color& a, float f) : Material(a, 0.9f, 128.0f), fuzz(f < 1 ? f : 1) {} // High specular, high shininess

    bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        // Reflect the ray around the normal
        Vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
        scattered = Ray(rec.p, reflected + fuzz * Vec3::random(-1, 1));
        attenuation = albedo;
        return (dot(scattered.direction(), rec.normal) > 0);
    }
};

// Area-style light source (emits radiance; does not scatter secondary rays in the CPU path tracer)
class Emissive : public Material {
public:
    Color emit;

    Emissive(const Color& e) : Material(Color(0.0f, 0.0f, 0.0f), 0.0f, 1.0f), emit(e) {}

    bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const override {
        (void)r_in;
        (void)rec;
        (void)attenuation;
        (void)scattered;
        return false;
    }
};

class Dielectric : public Material {
public:
    float ir; // Index of refraction (e.g., 1.5 for glass, 1.0 for air, 2.4 for diamond)

    Dielectric(float index_of_refraction) : Material(Color(1.0, 1.0, 1.0), 0.5f, 64.0f), ir(index_of_refraction) {} // Moderate specular like glass

    bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        attenuation = Color(1.0, 1.0, 1.0); // Glass absorbs nothing
        float refraction_ratio = rec.front_face ? (1.0f / ir) : ir;

        Vec3 unit_direction = unit_vector(r_in.direction());
        float cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0f);
        float sin_theta = sqrt(1.0f - cos_theta * cos_theta);

        // Check for total internal reflection
        bool cannot_refract = refraction_ratio * sin_theta > 1.0f;

        // Generate random float for Fresnel effect
        static thread_local std::mt19937 generator(std::random_device{}());
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

        if (cannot_refract || reflectance(cos_theta, refraction_ratio) > distribution(generator)) {
            // Total internal reflection or Fresnel reflection - reflect the ray
            Vec3 reflected = reflect(unit_direction, rec.normal);
            scattered = Ray(rec.p, reflected);
        } else {
            // Refract the ray (Snell's Law)
            Vec3 refracted = refract(unit_direction, rec.normal, refraction_ratio);
            scattered = Ray(rec.p, refracted);
        }

        return true;
    }

private:
    // Schlick's approximation for Fresnel effect
    float reflectance(float cosine, float ref_idx) const {
        // Use Schlick's approximation for reflectance
        float r0 = (1 - ref_idx) / (1 + ref_idx);
        r0 = r0 * r0;
        return r0 + (1 - r0) * pow((1 - cosine), 5);
    }

    // Snell's Law refraction
    Vec3 refract(const Vec3& uv, const Vec3& n, float etai_over_etat) const {
        float cos_theta = fmin(dot(-uv, n), 1.0f);
        Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
        float r_out_perp_len_sq = dot(r_out_perp, r_out_perp);
        Vec3 r_out_parallel = -sqrt(fabs(1.0f - r_out_perp_len_sq)) * n;
        return r_out_perp + r_out_parallel;
    }
};

#endif // MATERIAL_H
