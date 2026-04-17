#ifndef DOF_SHOWCASE_H
#define DOF_SHOWCASE_H

#include "scene.h"
#include "primitives/sphere.h"
#include "material/material.h"
#include "math/vec3.h"

// Depth of Field showcase scene.
// Spheres are arranged in a line receding into the distance.
// With DOF enabled and focus set to z~3, the mid-range spheres are sharp
// while close and distant ones blur. Metallic and glass materials catch
// specular highlights that exaggerate the bokeh effect.
//
// Camera suggested: position (0, 1, 8), lookat (0, 0.5, 0), vfov 30
// DOF: focus_distance 5.0, aperture 0.15-0.25

inline void setup_dof_showcase_scene(Scene& scene) {
    // Ground plane (neutral grey - large sphere trick)
    auto ground = std::make_shared<Lambertian>(Color(0.45, 0.45, 0.45));
    scene.add_object(std::make_shared<Sphere>(Point3(0, -100.5, 0), 100, ground));

    // Distant backdrop wall
    auto backdrop = std::make_shared<Lambertian>(Color(0.15, 0.18, 0.22));
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, -120), 100, backdrop));

    // -----------------------------------------------------------------------
    // ROW 1: Main depth line (centre)  z = 7, 5, 3, 1, -1, -3
    // Focus at z~3 — spheres at z=3 sharp, others blur.
    // -----------------------------------------------------------------------

    // Very close (z=7) — strong foreground blur at normal focus
    auto close_mat = std::make_shared<Metal>(Color(0.9, 0.3, 0.2), 0.05);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, 7), 0.5, close_mat));

    // Slightly close (z=5)
    auto near_mat = std::make_shared<Metal>(Color(0.9, 0.65, 0.2), 0.08);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, 5), 0.5, near_mat));

    // IN FOCUS (z=3) — main subject, polished gold
    auto focus_mat = std::make_shared<Metal>(Color(1.0, 0.85, 0.35), 0.02);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, 3), 0.65, focus_mat));

    // Mid-far (z=1) — beginning to blur
    auto midfar_mat = std::make_shared<Metal>(Color(0.4, 0.7, 0.9), 0.1);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, 1), 0.5, midfar_mat));

    // Far (z=-1)
    auto far_mat = std::make_shared<Metal>(Color(0.5, 0.85, 0.5), 0.12);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, -1), 0.5, far_mat));

    // Very far (z=-3) — strong background blur
    auto vfar_mat = std::make_shared<Lambertian>(Color(0.7, 0.4, 0.8));
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, -3), 0.5, vfar_mat));

    // -----------------------------------------------------------------------
    // ROW 2: Flanking row (x=+2) — adds visual depth layers
    // -----------------------------------------------------------------------
    auto flank_a = std::make_shared<Metal>(Color(0.95, 0.95, 0.98), 0.03);
    scene.add_object(std::make_shared<Sphere>(Point3(2, 0.3, 6), 0.4, flank_a));

    auto flank_b = std::make_shared<Lambertian>(Color(0.8, 0.3, 0.3));
    scene.add_object(std::make_shared<Sphere>(Point3(2, 0.3, 3.5), 0.4, flank_b));

    auto flank_c = std::make_shared<Metal>(Color(0.85, 0.55, 0.35), 0.15);
    scene.add_object(std::make_shared<Sphere>(Point3(2, 0.3, 1), 0.4, flank_c));

    auto flank_d = std::make_shared<Lambertian>(Color(0.3, 0.4, 0.8));
    scene.add_object(std::make_shared<Sphere>(Point3(2, 0.3, -2), 0.4, flank_d));

    // -----------------------------------------------------------------------
    // ROW 3: Flanking row (x=-2)
    // -----------------------------------------------------------------------
    auto flank_e = std::make_shared<Lambertian>(Color(0.4, 0.75, 0.4));
    scene.add_object(std::make_shared<Sphere>(Point3(-2, 0.3, 6), 0.4, flank_e));

    auto flank_f = std::make_shared<Metal>(Color(0.98, 0.78, 0.35), 0.06);
    scene.add_object(std::make_shared<Sphere>(Point3(-2, 0.3, 3.5), 0.4, flank_f));

    auto flank_g = std::make_shared<Lambertian>(Color(0.85, 0.55, 0.3));
    scene.add_object(std::make_shared<Sphere>(Point3(-2, 0.3, 1), 0.4, flank_g));

    auto flank_h = std::make_shared<Metal>(Color(0.7, 0.7, 0.75), 0.2);
    scene.add_object(std::make_shared<Sphere>(Point3(-2, 0.3, -2), 0.4, flank_h));

    // -----------------------------------------------------------------------
    // Lights — warm key + cool fill for pleasing bokeh colours
    // -----------------------------------------------------------------------
    scene.add_light(Light(Point3(3, 12, 5),  Color(1.0, 0.95, 0.9) * 1.2));   // warm overhead key
    scene.add_light(Light(Point3(-8, 6, 8),  Color(0.7, 0.85, 1.0) * 0.5));   // cool left fill
    scene.add_light(Light(Point3(0, 5, -8),  Color(1.0, 0.95, 0.85) * 0.35)); // back rim
}

#endif // DOF_SHOWCASE_H
