#ifndef FX_SHOWCASE_H
#define FX_SHOWCASE_H

#include "scene.h"
#include "primitives/sphere.h"
#include "material/material.h"
#include "math/vec3.h"

// FX showcase scene: designed to make each visual effect obvious.
//
//  - Chromatic Aberration: bright metallic spheres near screen edges create
//    strong specular highlights that split into R/G/B fringing.
//
//  - Bloom: highly polished chrome spheres reflect the bright key light,
//    producing overexposed highlights that bloom outward.
//
//  - Lens Flares: light source positioned at upper-right where the shader
//    calculates lens flare geometry (light_position uniform).
//
//  - Motion Blur: move the camera with WASD to see directional trails.
//
//  - TAA: look at the thin ring of small spheres around the centrepiece —
//    aliased without TAA, smooth with it.
//
// Camera suggested: position (0, 2, 8), lookat (0, 0, 0), vfov 45

inline void setup_fx_showcase_scene(Scene& scene) {
    // Ground — checkered for TAA aliasing test
    auto ground = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    // Use the built-in checkerboard texture material type (4)
    // We pass a Lambertian; the shader overrides it with checkerboard for mat==4
    // Use Metal so the shader stays in PBR path (checkerboard needs mat type 4)
    // Actually, set material via texture type flag — use regular lambertian for ground
    // and put a dedicated checker sphere as the floor
    scene.add_object(std::make_shared<Sphere>(Point3(0, -100.5, 0), 100, ground));

    // -----------------------------------------------------------------------
    // CHROME CENTREPIECE — strong bloom source
    // Very polished chrome sphere at origin; catches the key light and blooms.
    // -----------------------------------------------------------------------
    auto chrome = std::make_shared<Metal>(Color(0.97, 0.97, 0.99), 0.01);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, 0), 0.8, chrome));

    // -----------------------------------------------------------------------
    // BRIGHT METALLIC RING — chromatic aberration targets
    // Six spheres in a ring at radius 2.5; screen-edge spheres show the
    // strongest lens dispersion fringing.
    // -----------------------------------------------------------------------
    float ring_r = 2.5f;
    // Gold
    auto gold = std::make_shared<Metal>(Color(1.0, 0.82, 0.28), 0.04);
    scene.add_object(std::make_shared<Sphere>(Point3( ring_r, 0.3, 0), 0.4, gold));
    scene.add_object(std::make_shared<Sphere>(Point3(-ring_r, 0.3, 0), 0.4, gold));
    // Copper
    auto copper = std::make_shared<Metal>(Color(0.9, 0.55, 0.3), 0.06);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.3,  ring_r), 0.4, copper));
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.3, -ring_r), 0.4, copper));
    // Silver
    auto silver = std::make_shared<Metal>(Color(0.95, 0.95, 0.96), 0.03);
    scene.add_object(std::make_shared<Sphere>(Point3( ring_r*0.7f, 0.3,  ring_r*0.7f), 0.35, silver));
    scene.add_object(std::make_shared<Sphere>(Point3(-ring_r*0.7f, 0.3, -ring_r*0.7f), 0.35, silver));

    // -----------------------------------------------------------------------
    // SMALL SPHERE RING — TAA aliasing target
    // Thin row of small spheres; without TAA their silhouettes alias badly.
    // -----------------------------------------------------------------------
    float taa_r = 1.5f;
    auto taa_mat = std::make_shared<Lambertian>(Color(0.9, 0.9, 0.9));
    for (int i = 0; i < 12; i++) {
        float angle = float(i) / 12.0f * 6.28318f;
        float x = taa_r * cos(angle);
        float z = taa_r * sin(angle);
        scene.add_object(std::make_shared<Sphere>(Point3(x, 0.1, z), 0.12, taa_mat));
    }

    // -----------------------------------------------------------------------
    // BACKGROUND — diffuse coloured spheres add environment colour and depth
    // -----------------------------------------------------------------------
    auto bg_red  = std::make_shared<Lambertian>(Color(0.75, 0.2, 0.2));
    auto bg_blue = std::make_shared<Lambertian>(Color(0.2, 0.35, 0.75));
    auto bg_grn  = std::make_shared<Lambertian>(Color(0.2, 0.65, 0.3));
    scene.add_object(std::make_shared<Sphere>(Point3(-5, 1, -4), 1.2, bg_red));
    scene.add_object(std::make_shared<Sphere>(Point3( 5, 1, -4), 1.2, bg_blue));
    scene.add_object(std::make_shared<Sphere>(Point3( 0, 1, -6), 1.4, bg_grn));

    // Small accent spheres on the ground to extend the scene
    auto acc1 = std::make_shared<Metal>(Color(0.8, 0.6, 0.9), 0.2);
    auto acc2 = std::make_shared<Metal>(Color(0.6, 0.9, 0.8), 0.15);
    scene.add_object(std::make_shared<Sphere>(Point3(-3.5, 0.2, 2), 0.35, acc1));
    scene.add_object(std::make_shared<Sphere>(Point3( 3.5, 0.2, 2), 0.35, acc2));

    // -----------------------------------------------------------------------
    // Lights
    // Key light upper-right — used by lens flare (light_position maps here).
    // Fill + rim for full coverage.
    // -----------------------------------------------------------------------
    scene.add_light(Light(Point3(6, 14, 6),  Color(1.0, 0.97, 0.92) * 1.5));  // bright key (bloom source)
    scene.add_light(Light(Point3(-8, 8, 8),  Color(0.8, 0.9, 1.0) * 0.45));   // cool fill
    scene.add_light(Light(Point3(0, 4, -10), Color(1.0, 0.92, 0.85) * 0.35)); // back rim
}

#endif // FX_SHOWCASE_H
