# GPU Ray Tracer - Showcase Scene Guide

## Quick Start: Using Existing Showcase Scenes

Your GPU ray tracer already has **3 built-in showcase scenes** designed to show off different features:

### 1. Cornell Box (Classic Test Scene)
```bash
make interactive-gpu GPU_SCENE=cornell_box
./build/raytracer_interactive_gpu
```

**Best for:**
- Color bleeding demonstrations (GI)
- Reflection quality testing
- Classic ray tracing comparison
- Indoor scene lighting

**Showcase Features:**
- Red/green walls for color bleeding
- Metallic spheres for reflections
- Glass sphere for refraction
- Various materials (Lambertian, metal, dielectric)
- Procedural textures

### 2. GPU Demo (PBR Material Showcase)
```bash
make interactive-gpu SCENE_NAME=gpu_demo
./build/raytracer_interactive_gpu
```

**Best for:**
- Material quality comparisons
- Roughness demonstrations
- Metallic vs dielectric comparisons
- PBR lighting quality

**Showcase Features:**
- Row of metals from mirror to matte
- Metallic sweep from plastic to pure metal
- Gold and copper demonstration spheres
- Colorful spheres for lighting tests

### 3. PBR Showcase (Material Quality Sweep)
```bash
make interactive-gpu SCENE_NAME=pbr_showcase
./build/raytracer_interactive_gpu
```

**Best for:**
- Comprehensive material testing
- Roughness from 0.0 to 1.0
- Metallic from 0.0 to 1.0
- Professional material comparisons

**Showcase Features:**
- 7-sphere roughness sweep (mirror to matte)
- Metallic sweep (plastic to pure metal)
- Gold and copper examples
- Colorful plastic spheres

---

## Creating Your Own Showcase Scene

### Method 1: Modify Existing Cornell Box

Edit `src/scene/cornell_box.h` to create a showcase:

```cpp
// Add a metallic sphere row for reflection showcase
auto gold_sphere = std::make_shared<Metal>(Color3(1.0, 0.85, 0.3), 0.1);
scene.add(std::make_shared<Sphere>(Point3(0, 1, 0), 0.8, gold_sphere));

// Add a colorful sphere for color bleeding
auto red_sphere = std::make_shared<Lambertian>(Color3(0.8, 0.1, 0.1));
scene.add(std::make_shared<Sphere>(Point3(2, 0.5, -1), 0.4, red_sphere));

// Add a mirror sphere
auto mirror_mat = std::make_shared<Metal>(Color3(0.98, 0.98, 0.98), 0.0);
scene.add(std::make_shared<Sphere>(Point3(-2, 0.6, 1), 0.5, mirror_mat));
```

### Method 2: Create Custom Scene File

Create `src/scene/my_showcase.h`:

```cpp
#ifndef MY_SHOWCASE_H
#define MY_SHOWCASE_H

#include "scene.h"
#include "primitives/sphere.h"
#include "material/material.h"
#include "math/vec3.h"

inline void setup_my_showcase_scene(Scene& scene) {
    // Ground
    auto ground = std::make_shared<Lambertian>(Color3(0.5, 0.5, 0.5));
    scene.add(std::make_shared<Sphere>(Point3(0, -100.5, 0), 100, ground));

    // Back wall
    auto wall = std::make_shared<Lambertian>(Color3(0.8, 0.8, 0.8));
    scene.add(std::make_shared<Sphere>(Point3(0, 0, -105), 100, wall));

    // Showcase objects
    auto gold = std::make_shared<Metal>(Color3(1.0, 0.85, 0.3), 0.1);
    scene.add(std::make_shared<Sphere>(Point3(0, 1, 0), 1.0, gold));

    auto copper = std::make_shared<Metal>(Color3(0.95, 0.60, 0.20), 0.1);
    scene.add(std::make_shared<Sphere>(Point3(-2, 0.8, 1), 0.7, copper));

    auto chrome = std::make_shared<Metal>(Color3(0.98, 0.98, 0.98), 0.0);
    scene.add(std::make_shared<Sphere>(Point3(2, 0.6, -1), 0.6, chrome));
}

#endif
```

Then modify `src/main_gpu_interactive.cpp`:
1. Add include: `#include "scene/my_showcase.h"`
2. Add scene check in `setup_scene_data()`:
```cpp
else if (scene_name == "my_showcase") {
    setup_my_showcase_scene(scene);
}
```

Build with:
```bash
make interactive-gpu SCENE_NAME=my_showcase
```

---

## Perfect Camera Positions for Showcase

### Cornell Box Best Views

**Front View (Classic):**
```
Position: (0, 1, 6)
Look at: (0, 1, 0)
Shows: Entire room, all objects
```

**Corner View (Depth):**
```
Position: (-4, 2, 4)
Look at: (0, 1, 0)
Shows: 3D depth, color bleeding
```

**Close-up on Metals:**
```
Position: (0, 1, 2)
Look at: (0, 1, -1)
Shows: Metallic sphere details
```

**Reflection Testing:**
```
Position: (2, 0.8, 2)
Look at: (0, 0.8, 0)
Shows: Mirror sphere reflections
```

### GPU Demo Best Views

**Roughness Sweep (Front):**
```
Position: (-4, 1, 6)
Look at: (-1, 0.5, -2)
Shows: All roughness levels
```

**Metallic Sweep (Angled):**
```
Position: (3, 1, 5)
Look at: (3, 0, 2)
Shows: Metallic comparison
```

**Gold Sphere (Close-up):**
```
Position: (6, 0.5, 4)
Look at: (6, 0.5, 0)
Shows: Gold reflections and lighting
```

### PBR Showcase Best Views

**Full Scene (Overview):**
```
Position: (0, 2, 8)
Look at: (0, 0.5, 0)
Shows: All material comparisons
```

**Roughness Sweep (Detailed):**
```
Position: (-2, 0.5, 5)
Look at: (-1.5, 0.5, 0)
Shows: Roughness progression
```

---

## Feature Combinations for Maximum Quality

### Showcase Quality Presets

```bash
# Maximum quality showcase
make phase4-complete \
    SCENE_NAME=pbr_showcase

# Or interactively
make phase4-complete SCENE_NAME=pbr_showcase
./build/raytracer_interactive_gpu
```

### Best Feature Combinations by Scene Type

#### Material Showcase
```bash
# Enable all reflection features
make phase4-complete SCENE_NAME=pbr_showcase

# In application:
# Press Shift+S - Enable SSR (essential for materials)
# Press E - Enable Environment
# Press O - Enable SSAO (adds depth)
# Press V - Enable Vignette (cinematic)
# Press T - Cycle to "ACES" tone mapping

# Position for close-up shots
# Move: WASD to get close (1-2 units away)
# Look: Mouse to center on metallic spheres
# Press S - Screenshot
```

#### Color Bleeding Showcase
```bash
# Use Cornell Box for best color bleeding
make phase4-complete SCENE_NAME=cornell_box

# In application:
# Press G - Enable GI (essential for color bleeding)
# Press Shift+S - Enable SSR (adds reflections)
# Press [ ] ] - Set GI samples to 8 (maximum)
# Press - = - Set GI intensity to 0.6-0.8

# Position to see red wall bleeding
# Position: (-2, 1, 4)
# Look at: (0, 1, 0)
# Press S - Screenshot
```

#### Reflection Quality Showcase
```bash
# Use GPU Demo for reflection testing
make phase4-complete SCENE_NAME=gpu_demo

# In application:
# Press Shift+S - Enable SSR
# Press , . - Increase SSR samples to 32 (maximum)
# Press / - Reduce roughness cutoff to 0.3 (more surfaces reflective)
# Press P - Switch to PBR lighting

# Position close to metallic objects
# Move: WASD to get very close (0.5-1 unit)
# Look: Mouse at polished metal spheres
# Press S - Screenshot
```

#### Cinematic Quality Showcase
```bash
# Maximum cinematic settings
make phase4-complete SCENE_NAME=pbr_showcase

# In application:
# Press G - Enable GI
# Press Shift+S - Enable SSR
# Press E - Enable Environment
# Press V - Enable Vignette
# Press N - Enable Film Grain
# Press T - Cycle tone mapping to "Filmic"
# Press 2 - Increase exposure slightly
# Press 4 - Increase contrast slightly

# Position for dramatic shots
# Position: Low angle (Position: (2, 0.3, 5))
# Look at: Slightly upward (Look at: (0, 1, 0))
# Press S - Screenshot
```

---

## Screenshot Workflow for Professional Results

### Step-by-Step Showcase Screenshot Guide

**1. Build Maximum Quality:**
```bash
make phase4-complete SCENE_NAME=pbr_showcase
```

**2. Launch Application:**
```bash
./build/raytracer_interactive_gpu
```

**3. Position Camera:**
- Use **WASD** to move to position: `(0, 2, 8)`
- Use **Mouse** to look at: `(0, 0.5, 0)`
- Find angle where all materials are visible

**4. Enable Features:**
- Press **G** - Enable GI
- Press **Shift+S** - Enable SSR
- Press **E** - Enable Environment
- Press **V** - Enable Vignette
- Press **T** - Cycle tone mapping to find best look

**5. Fine-Tune Quality:**
- Press **[ / ]** - Adjust GI samples (try 8)
- Press **- / =** - Adjust GI intensity (try 0.6)
- Press **,** / **.** - Adjust SSR samples (try 24)
- Press **1-6** - Adjust exposure/contrast/saturation

**6. Capture Screenshot:**
- Press **S** to save
- Find in `screenshots/screenshot_*.png`
- Rename to descriptive name: `mv screenshot_*.png screenshots/material_showcase.png`

### Advanced Screenshot Techniques

**Multi-Feature Comparison:**

1. **Baseline (No advanced features):**
   - Just run, no feature toggles
   - Screenshot: `baseline.png`

2. **+ GI only:**
   - Press **G**
   - Screenshot: `with_gi.png`

3. **+ SSR:**
   - Press **Shift+S**
   - Screenshot: `with_gi_ssr.png`

4. **+ Environment:**
   - Press **E**
   - Screenshot: `complete.png`

**Before/After Comparisons:**
```bash
# Create comparison montage
montage -tile 2x2 -geometry 800x450+2+2 \
    screenshots/baseline.png \
    screenshots/with_gi.png \
    screenshots/with_gi_ssr.png \
    screenshots/complete.png \
    screenshots/showcase_comparison.png
```

---

## Scene Design Principles for Showcase

### 1. Feature-Focused Design

**For GI/Color Bleeding:**
- Use adjacent colored surfaces
- Red wall next to white floor
- Bright objects near dark surfaces
- Example: Cornell Box

**For SSR/Reflections:**
- Include metallic objects
- Vary roughness levels
- Place objects to reflect each other
- Add light sources for reflections

**For Environment:**
- Outdoor scenes with sky
- Ground plane for horizon
- Elevated camera positions

### 2. Material Showcase Design

**Roughness Sweep:**
- 7 spheres: roughness 0.0, 0.1, 0.2, 0.3, 0.5, 0.7, 0.9
- All metallic (same metal)
- Same size, aligned
- Easy comparison

**Metallic Sweep:**
- 4 spheres: metallic 0.0, 0.3, 0.6, 1.0
- Same roughness
- Shows dielectric to metal transition

**Color Variety:**
- Red, Green, Blue spheres
- Shows color rendering
- Tests white balance

### 3. Lighting Design

**Three-Point Lighting:**
```cpp
// Main light
Point3 main_light(0.0, 18.0, 0.0);

// Fill light
Point3 fill_light(-10.0, 10.0, 10.0);

// Rim light
Point3 rim_light(15.0, 5.0, -5.0);
```

**Dramatic Lighting:**
- Single strong light source
- Deep shadows for contrast
- Highlights material properties

---

## Quick Showcase Presets

### Material Quality Showcase
```bash
make phase4-complete SCENE_NAME=pbr_showcase
# Position: (0, 2, 8) looking at (0, 0.5, 0)
# Features: G, Shift+S, E, O, V, N
# Tone mapping: ACES
```

### Color Bleeding Showcase
```bash
make phase4-complete SCENE_NAME=cornell_box
# Position: (-2, 1, 4) looking at (0, 1, 0)
# Features: G (8 samples, 0.8 intensity)
# Shows red wall → floor bleeding
```

### Reflection Quality Showcase
```bash
make phase4-complete SCENE_NAME=gpu_demo
# Position: (-4, 1, 6) looking at (-1, 0.5, -2)
# Features: Shift+S (32 samples), SSR cutoff 0.3
# Shows mirror to matte progression
```

### Cinematic Showcase
```bash
make phase4-complete SCENE_NAME=cornell_box
# Position: (2, 0.5, 5) looking at (0, 1, 0)
# Features: G, Shift+S, E, V, N, B
# Tone mapping: Filmic
# Exposure: 1.2, Contrast: 1.1
```

---

## Troubleshooting Showcase Scenes

### Scene Not Loading
```bash
# Check scene name is correct
make interactive-gpu GPU_SCENE=cornell_box

# Available scenes:
# - cornell_box (default)
# - gpu_demo
# - pbr_showcase
```

### Materials Look Flat
- Press **P** to switch to PBR lighting
- Press **L** to cycle to 3-point lighting
- Enable **Shift+S** for reflections
- Check material definitions

### No Color Bleeding
- Press **G** to enable GI
- Press **[ / ]** to increase GI samples
- Press **- / =** to increase GI intensity
- Verify scene has adjacent colored surfaces

### Reflections Not Visible
- Press **Shift+S** to enable SSR
- Press **,** / **.** to increase SSR samples
- Press **/** to reduce roughness cutoff
- Move closer to metallic objects
- Check scene has metallic materials

---

## Advanced: Creating Comparison Scenes

### Side-by-Side Material Comparison

```cpp
// Left side: Low roughness metals
for (int i = 0; i < 5; i++) {
    float roughness = 0.05 + float(i) * 0.05;
    auto metal = std::make_shared<Metal>(Color3(0.9, 0.9, 0.9), roughness);
    scene.add(std::make_shared<Sphere>(Point3(-4.0 + i * 0.8, 1.0, 0), 0.4, metal));
}

// Right side: High roughness metals
for (int i = 0; i < 5; i++) {
    float roughness = 0.5 + float(i) * 0.1;
    auto metal = std::make_shared<Metal>(Color3(0.9, 0.9, 0.9), roughness);
    scene.add(std::make_shared<Sphere>(Point3(4.0 + i * 0.8, 1.0, 0), 0.4, metal));
}
```

### Color Calibration Scene

```cpp
// Add color spheres for calibration
scene.add(std::make_shared<Sphere>(Point3(-2, 0.5, 0), 0.3,
    std::make_shared<Lambertian>(Color3(1.0, 0.0, 0.0))));  // Red

scene.add(std::make_shared<Sphere>(Point3(0, 0.5, 0), 0.3,
    std::make_shared<Lambertian>(Color3(0.0, 1.0, 0.0))));  // Green

scene.add(std::make_shared<Sphere>(Point3(2, 0.5, 0), 0.3,
    std::make_shared<Lambertian>(Color3(0.0, 0.0, 1.0))));  // Blue
```

---

## Example: Complete Showcase Workflow

```bash
# 1. Build maximum quality showcase
make phase4-complete SCENE_NAME=pbr_showcase

# 2. Run the application
./build/raytracer_interactive_gpu

# 3. Position camera for overview
# Move to: (0, 2, 8)
# Look at: (0, 0.5, 0)

# 4. Enable all features
# Press: G (GI), Shift+S (SSR), E (Env), O (SSAO), V (Vignette), N (Film Grain)
# Press: T (cycle to "ACES")

# 5. Fine-tune quality
# Press: ] ] ] (8 GI samples)
# Press: = = (increase GI intensity)
# Press: . . (increase SSR samples)

# 6. Move to close-up
# Press: W W W (get closer)
# Look: At center metallic sphere

# 7. Capture screenshot
# Press: S
# Result: screenshots/screenshot_*.png

# 8. Organize screenshots
mv screenshots/screenshot_*.png screenshots/pbr_showcase_complete.png
```

---

## Quick Reference Commands

```bash
# Build with specific scene
make interactive-gpu GPU_SCENE=cornell_box
make interactive-gpu SCENE_NAME=gpu_demo
make interactive-gpu SCENE_NAME=pbr_showcase

# Maximum quality builds
make phase4-complete SCENE_NAME=pbr_showcase
make phase35-complete SCENE_NAME=gpu_demo

# Run application
./build/raytracer_interactive_gpu

# Screenshot locations
ls -la screenshots/
```

---

**Pro Tips:**
- Start with `pbr_showcase` - designed for showing off materials
- Use `cornell_box` for color bleeding demonstrations
- Use `gpu_demo` for comprehensive feature testing
- Always enable **G** + **Shift+S** + **E** for maximum quality
- Use **T** to cycle tone mapping operators for different looks
- **Vignette** adds instant cinematic feel with very low cost
- Position camera to show multiple materials in one shot
- Get close (1-2 units) for material detail shots
- Get far (6-8 units) for overview shots

**Quality Check:**
- ✅ Multiple materials visible? (yes = pbr_showcase)
- ✅ Metallic objects reflecting? (yes = enable SSR)
- ✅ Color bleeding visible? (yes = enable GI, use cornell_box)
- ✅ Cinematic look? (yes = enable vignette, use filmic tone mapping)
- ✅ Professional quality? (yes = phase4-complete preset)
