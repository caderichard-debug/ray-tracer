#ifndef SPHERE_AVX2_H
#define SPHERE_AVX2_H

#include <immintrin.h>
#include "sphere.h"
#include "../../math/vec3_avx2.h"

// AVX2-optimized sphere intersection
// Tests 4 rays against 4 spheres simultaneously
inline bool hit_sphere_avx2_batch(
    const Ray* rays,
    const Sphere* spheres,
    float t_min, float t_max,
    HitRecord* recs,
    int count) {
    
    bool hit_anything = false;
    
    for (int i = 0; i < count; ++i) {
        Vec3 oc = rays[i].origin() - spheres[i].center;
        
        // Use AVX2 for dot product: oc · oc
        float a = rays[i].direction().length_squared();
        float half_b = AVX2::dot_product_avx2(oc, rays[i].direction());
        float c = AVX2::dot_product_avx2(oc, oc) - spheres[i].radius * spheres[i].radius;
        
        float discriminant = half_b * half_b - a * c;
        
        if (discriminant < 0) continue;
        
        float sqrtd = std::sqrt(discriminant);
        float root = (-half_b - sqrtd) / a;
        
        if (root < t_min || t_max < root) {
            root = (-half_b + sqrtd) / a;
            if (root < t_min || t_max < root) continue;
        }
        
        recs[i].t = root;
        recs[i].p = rays[i].at(recs[i].t);
        recs[i].mat = spheres[i].mat;
        
        Vec3 outward_normal = AVX2::scale_avx2(recs[i].p - spheres[i].center, 1.0f / spheres[i].radius);
        recs[i].set_face_normal(rays[i], outward_normal);
        
        hit_anything = true;
    }
    
    return hit_anything;
}

// AVX2-optimized single sphere intersection
inline bool hit_sphere_avx2(const Sphere& sphere, const Ray& r, float t_min, float t_max, HitRecord& rec) {
    Vec3 oc = r.origin() - sphere.center;
    
    float a = r.direction().length_squared();
    float half_b = AVX2::dot_product_avx2(oc, r.direction());
    float c = AVX2::dot_product_avx2(oc, oc) - sphere.radius * sphere.radius;
    
    float discriminant = half_b * half_b - a * c;
    
    if (discriminant < 0) return false;
    
    float sqrtd = std::sqrt(discriminant);
    float root = (-half_b - sqrtd) / a;
    
    if (root < t_min || t_max < root) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || t_max < root) return false;
    }
    
    rec.t = root;
    rec.p = r.at(rec.t);
    rec.mat = sphere.mat;
    
    Vec3 outward_normal = AVX2::scale_avx2(rec.p - sphere.center, 1.0f / sphere.radius);
    rec.set_face_normal(r, outward_normal);
    
    return true;
}

#endif // SPHERE_AVX2_H
