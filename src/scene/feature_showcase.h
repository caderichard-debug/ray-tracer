#ifndef FEATURE_SHOWCASE_H
#define FEATURE_SHOWCASE_H

#include "scene.h"
#include "light.h"
#include "material/material.h"
#include "texture/texture.h"
#include "primitives/sphere.h"
#include "primitives/triangle.h"
#include "math/vec3.h"
#include <memory>
#include <algorithm>

// One gallery scene designed to exercise the GPU interactive feature set.
// Default camera (0, 2, 15) → (0, 0, 0) frames the layout; walk slightly with WASD
// to trigger motion blur. Suggested toggles (build with usual flags, then in-app):
//
//   I  — progressive path tracing + emissive ceiling (hold still to converge)
//   P  — Phong vs PBR
//   L  — cycle 1–3 lights (shadows / contact)
//   F  — depth of field (focus ~3, aperture ~0.12–0.18)
//   M  — motion blur + WASD
//   R  — reflections (metal + glass Fresnel / Snell)
//   T  — tone mapping; B bloom; N grain; V vignette; O SSAO
//   Shift+S SSR, E env map, G GI
//   (Rebuild with TAA / chromatic / DOF Makefile flags for those pipelines.)
//
inline void setup_feature_showcase_scene(Scene& scene) {
    scene.ambient_light = Color(0.08f, 0.08f, 0.1f);

    // -------------------------------------------------------------------------
    // Ground + distant backdrop (depth, shadows, SSR hints)
    // -------------------------------------------------------------------------
    auto ground = std::make_shared<Lambertian>(Color(0.42f, 0.4f, 0.38f));
    scene.add_object(std::make_shared<Sphere>(Point3(0.0f, -100.5f, 0.0f), 100.0f, ground));

    auto backdrop = std::make_shared<Lambertian>(Color(0.12f, 0.14f, 0.2f));
    scene.add_object(std::make_shared<Sphere>(Point3(0.0f, 0.0f, -120.0f), 100.0f, backdrop));

    // -------------------------------------------------------------------------
    // PROCEDURALS (GPU materials 4–7) — left aisle, receding in +Z
    // -------------------------------------------------------------------------
    auto checker_tex = std::make_shared<CheckerTexture>(
        std::make_shared<SolidColor>(Color(0.85f, 0.25f, 0.2f)),
        std::make_shared<SolidColor>(Color(0.15f, 0.25f, 0.85f)),
        10.0f);
    auto checker_mat = std::make_shared<Lambertian>(checker_tex);
    scene.add_object(std::make_shared<Sphere>(Point3(-4.35f, 0.42f, 2.4f), 0.48f, checker_mat));

    auto noise_tex = std::make_shared<NoiseTexture>(
        Color(1.0f, 1.0f, 1.0f), Color(0.05f, 0.05f, 0.08f), 6.0f, 5, 0.65f);
    auto noise_mat = std::make_shared<Lambertian>(noise_tex);
    scene.add_object(std::make_shared<Sphere>(Point3(-4.35f, 0.42f, 3.45f), 0.48f, noise_mat));

    Point3 g_center(-4.35f, 0.42f, 4.5f);
    float g_r = 0.48f;
    Point3 g_min(g_center.x - g_r, g_center.y - g_r, g_center.z - g_r);
    Point3 g_max(g_center.x + g_r, g_center.y + g_r, g_center.z + g_r);
    auto grad_tex = std::make_shared<GradientTexture>(
        Color(0.55f, 0.15f, 0.75f), Color(0.95f, 0.9f, 0.25f), Vec3(0, 1, 0), g_min, g_max);
    auto grad_mat = std::make_shared<Lambertian>(grad_tex);
    scene.add_object(std::make_shared<Sphere>(g_center, g_r, grad_mat));

    auto stripe_tex = std::make_shared<StripeTexture>(
        Color(0.85f, 0.5f, 0.15f), Color(0.92f, 0.92f, 0.95f), 9.0f, 0.0f);
    auto stripe_mat = std::make_shared<Lambertian>(stripe_tex);
    scene.add_object(std::make_shared<Sphere>(Point3(-4.35f, 0.42f, 5.55f), 0.48f, stripe_mat));

    // -------------------------------------------------------------------------
    // CHROME + GLASS + RED occluder — bloom, refraction, hard shadows
    // -------------------------------------------------------------------------
    auto chrome = std::make_shared<Metal>(Color(0.96f, 0.97f, 0.99f), 0.02f);
    scene.add_object(std::make_shared<Sphere>(Point3(-1.45f, 0.55f, 2.85f), 0.58f, chrome));

    auto glass = std::make_shared<Dielectric>(1.5f);
    scene.add_object(std::make_shared<Sphere>(Point3(2.05f, 0.52f, 3.15f), 0.52f, glass));

    auto behind = std::make_shared<Lambertian>(Color(0.85f, 0.12f, 0.1f));
    scene.add_object(std::make_shared<Sphere>(Point3(2.05f, 0.38f, 4.05f), 0.24f, behind));

    // -------------------------------------------------------------------------
    // METALS — perfect vs fuzzy (PBR roughness on GPU)
    // -------------------------------------------------------------------------
    auto gold = std::make_shared<Metal>(Color(1.0f, 0.82f, 0.32f), 0.04f);
    scene.add_object(std::make_shared<Sphere>(Point3(3.75f, 0.45f, 4.35f), 0.5f, gold));

    auto brushed = std::make_shared<Metal>(Color(0.72f, 0.74f, 0.78f), 0.32f);
    scene.add_object(std::make_shared<Sphere>(Point3(3.75f, 0.45f, 2.65f), 0.5f, brushed));

    // -------------------------------------------------------------------------
    // CHROMATIC ABERRATION targets — bright metals near frame edges
    // -------------------------------------------------------------------------
    auto edge_a = std::make_shared<Metal>(Color(0.98f, 0.92f, 0.35f), 0.03f);
    auto edge_b = std::make_shared<Metal>(Color(0.35f, 0.85f, 0.95f), 0.04f);
    scene.add_object(std::make_shared<Sphere>(Point3(5.65f, 0.48f, 2.55f), 0.42f, edge_a));
    scene.add_object(std::make_shared<Sphere>(Point3(-5.85f, 0.48f, 2.75f), 0.42f, edge_b));

    // -------------------------------------------------------------------------
    // DEPTH-OF-FIELD strip — z = 6 … 0 at fixed x (focus ~3 with F / aperture)
    // -------------------------------------------------------------------------
    auto dof_a = std::make_shared<Metal>(Color(0.9f, 0.35f, 0.25f), 0.08f);
    auto dof_b = std::make_shared<Metal>(Color(0.92f, 0.7f, 0.2f), 0.1f);
    auto dof_c = std::make_shared<Metal>(Color(1.0f, 0.86f, 0.38f), 0.03f);
    auto dof_d = std::make_shared<Lambertian>(Color(0.35f, 0.65f, 0.9f));
    auto dof_e = std::make_shared<Lambertian>(Color(0.45f, 0.85f, 0.45f));
    const float dof_x = -1.28f;
    const float dof_y = 0.38f;
    const float dof_r = 0.36f;
    scene.add_object(std::make_shared<Sphere>(Point3(dof_x, dof_y, 6.0f), dof_r, dof_a));
    scene.add_object(std::make_shared<Sphere>(Point3(dof_x, dof_y, 4.5f), dof_r, dof_b));
    scene.add_object(std::make_shared<Sphere>(Point3(dof_x, dof_y, 3.0f), dof_r, dof_c));
    scene.add_object(std::make_shared<Sphere>(Point3(dof_x, dof_y, 1.5f), dof_r, dof_d));
    scene.add_object(std::make_shared<Sphere>(Point3(dof_x, dof_y, 0.0f), dof_r, dof_e));

    // -------------------------------------------------------------------------
    // TAA stress — micro ring of lambertian spheres (high-frequency silhouettes)
    // -------------------------------------------------------------------------
    auto taa_mat = std::make_shared<Lambertian>(Color(0.92f, 0.92f, 0.94f));
    const float taa_cx = 0.15f, taa_cz = 3.55f, taa_rad = 1.05f;
    for (int i = 0; i < 10; ++i) {
        float ang = float(i) / 10.0f * 6.2831853f;
        float x = taa_cx + taa_rad * std::cos(ang);
        float z = taa_cz + taa_rad * std::sin(ang);
        scene.add_object(std::make_shared<Sphere>(Point3(x, 0.13f, z), 0.11f, taa_mat));
    }

    // -------------------------------------------------------------------------
    // EMISSIVE area light (path tracing + strong bounce light)
    // -------------------------------------------------------------------------
    auto lamp = std::make_shared<Emissive>(Color(3.2f, 3.0f, 2.6f));
    scene.add_object(std::make_shared<Sphere>(Point3(0.0f, 3.85f, 0.8f), 0.72f, lamp));

    // -------------------------------------------------------------------------
    // TRIANGLES — checker pyramid (mat 8) + gradient poster (mat 9)
    // -------------------------------------------------------------------------
    Point3 p_top(-4.15f, 2.35f, 4.55f);
    Point3 p_b1(-4.75f, 1.05f, 4.05f);
    Point3 p_b2(-3.55f, 1.05f, 4.05f);
    Point3 p_b3(-4.15f, 1.05f, 5.15f);
    auto pyr_check = std::make_shared<CheckerTexture>(
        std::make_shared<SolidColor>(Color(0.08f, 0.08f, 0.1f)),
        std::make_shared<SolidColor>(Color(0.92f, 0.92f, 0.94f)),
        7.0f);
    auto pyr_mat = std::make_shared<Lambertian>(pyr_check);
    scene.add_object(std::make_shared<Triangle>(p_top, p_b1, p_b2, pyr_mat));
    scene.add_object(std::make_shared<Triangle>(p_top, p_b2, p_b3, pyr_mat));
    scene.add_object(std::make_shared<Triangle>(p_top, p_b3, p_b1, pyr_mat));
    scene.add_object(std::make_shared<Triangle>(p_b1, p_b3, p_b2, pyr_mat));

    Point3 q_tl(-5.92f, 0.85f, 2.05f);
    Point3 q_tr(-5.92f, 2.25f, 2.05f);
    Point3 q_br(-5.92f, 2.25f, 3.95f);
    Point3 q_bl(-5.92f, 0.85f, 3.95f);
    Point3 q_min(
        std::min({q_tl.x, q_tr.x, q_br.x, q_bl.x}),
        std::min({q_tl.y, q_tr.y, q_br.y, q_bl.y}),
        std::min({q_tl.z, q_tr.z, q_br.z, q_bl.z}));
    Point3 q_max(
        std::max({q_tl.x, q_tr.x, q_br.x, q_bl.x}),
        std::max({q_tl.y, q_tr.y, q_br.y, q_bl.y}),
        std::max({q_tl.z, q_tr.z, q_br.z, q_bl.z}));
    auto sign_grad = std::make_shared<GradientTexture>(
        Color(1.0f, 0.15f, 0.12f), Color(0.1f, 0.15f, 1.0f), Vec3(0, 0, 1), q_min, q_max);
    auto sign_mat = std::make_shared<Lambertian>(sign_grad);
    scene.add_object(std::make_shared<Triangle>(q_tl, q_tr, q_br, sign_mat));
    scene.add_object(std::make_shared<Triangle>(q_tl, q_br, q_bl, sign_mat));

    // -------------------------------------------------------------------------
    // Lights — bright key for bloom; fill/rim read shape
    // -------------------------------------------------------------------------
    scene.add_light(Light(Point3(5.5f, 12.0f, 6.0f), Color(1.0f, 0.96f, 0.9f) * 1.35f));
    scene.add_light(Light(Point3(-6.5f, 7.5f, 3.0f), Color(0.75f, 0.88f, 1.0f) * 0.48f));
    scene.add_light(Light(Point3(0.0f, 5.0f, -5.0f), Color(1.0f, 0.94f, 0.88f) * 0.38f));
}

#endif // FEATURE_SHOWCASE_H
