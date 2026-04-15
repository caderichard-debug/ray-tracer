# 🎬 GPU Ray Tracer - Quick Showcase Reference

## Instant Showcase Scenes

### 3 Built-in Showcase Scenes

```bash
# 1. Material Quality Showcase (RECOMMENDED)
./showcase_scene.sh
# Choose: 3) PBR Showcase

# 2. Classic Cornell Box
./showcase_scene.sh
# Choose: 1) Cornell Box

# 3. PBR Material Demo
./showcase_scene.sh
# Choose: 2) GPU Demo
```

## Maximum Quality in 3 Steps

```bash
# 1. Build
make phase4-complete GPU_SCENE=pbr_showcase

# 2. Run
./build/raytracer_interactive_gpu

# 3. Enable Features & Screenshot
# Press: G + Shift+S + E + V + N (all features)
# Press: S (save screenshot)
```

## Perfect Screenshot Settings

### Material Showcase
``**Position:** (0, 2, 8)
**Look At:** (0, 0.5, 0)
**Features:** G, Shift+S, E, V, N
**Tone Map:** ACES
**GI Samples:** 8
**SSR Samples:** 24
```

### Color Bleeding Demo
```
**Position:** (-2, 1, 4)
**Look At:** (0, 1, 0)
**Scene:** Cornell Box
**Features:** G (8 samples, 0.8 intensity)
**Shows:** Red wall → floor color transfer
```

### Reflection Quality Demo
```
**Position:** (-4, 1, 6)
**Look At:** (-1, 0.5, -2)
**Scene:** GPU Demo
**Features:** Shift+S (32 samples, cutoff 0.3)
**Shows:** Mirror → matte metal sweep
```

## Keyboard Shortcuts for Showcase

**Essential Features:**
- **G** - Enable GI (color bleeding)
- **Shift+S** - Enable SSR (reflections)
- **E** - Enable Environment (sky)
- **V** - Enable Vignette (cinematic)

**Quality Adjustments:**
- **[ / ]** - GI samples (1-8)
- **- / =** - GI intensity (0.0-1.0)
- **,** / **.** - SSR samples (4-32)
- **T** - Cycle tone mapping

**Capture:**
- **S** - Save screenshot
- **H** - Help overlay

## Camera Movement

- **WASD** - Move forward/left/backward/right
- **Arrow Keys** - Move up/down
- **Mouse** - Look around (click to capture)
- **Left Click** - Capture/release mouse

## Pro Tips

✅ **Best Scene:** `pbr_showcase` - Designed for showcasing
✅ **Best Position:** (0, 2, 8) looking at (0, 0.5, 0) - Overview
✅ **Best Features:** G + Shift+S + E + V - Maximum quality
✅ **Best Tone Map:** ACES (default) or Filmic (cinematic)
✅ **Close-up:** W key to get within 1-2 units of objects
✅ **Screenshot:** S key saves to `screenshots/` folder

## Scene-Specific Tips

### Cornell Box
- **Best for:** Color bleeding, reflections
- **Key Features:** Red/green walls, metallic spheres, glass
- **Showcase GI:** Position near red wall, enable G
- **Showcase Reflections:** Position near metals, enable Shift+S

### GPU Demo
- **Best for:** Material comparisons, roughness testing
- **Key Features:** Roughness sweep, metallic sweep
- **Showcase Roughness:** Look at row of spheres (left side)
- **Showcase Metallic:** Look at colored spheres (right side)

### PBR Showcase
- **Best for:** Comprehensive material quality
- **Key Features:** 7-sphere roughness sweep, 4-sphere metallic sweep
- **Showcase All Materials:** Position far back (0, 2, 8)
- **Showcase Details:** Move close to individual spheres

## Quality Presets

```bash
# Fast (Testing)
make phase4-complete  # 15-20 FPS

# Quality (Screenshots)
make phase4-complete  # All features enabled

# Alternative Presets
make gi-balanced      # GI only, 35 FPS
make ssr-quality     # SSR + Phase 3.5
make cinematic-quality # Cinematic post-processing
```

## Screenshot Workflow

1. **Build** scene: `make phase4-complete GPU_SCENE=pbr_showcase`
2. **Launch:** `./build/raytracer_interactive_gpu`
3. **Position:** Move to (0, 2, 8), look at (0, 0.5, 0)
4. **Enable features:** Press G, Shift+S, E, V, N
5. **Fine-tune:** Use [, ], -, =, ,, . keys
6. **Capture:** Press S
7. **Find:** `ls screenshots/screenshot_*.png`

## Troubleshooting

**No scene loaded?**
- Check SCENE_NAME matches available scenes
- Try: `make interactive-gpu SCENE_NAME=cornell_box`

**Materials look flat?**
- Press **P** to switch to PBR lighting
- Press **L** to cycle to 3-point lighting
- Enable **Shift+S** for reflections

**No color bleeding?**
- Use Cornell Box scene
- Press **G** to enable GI
- Increase GI samples: Press **]** key
- Increase GI intensity: Press **=** key

**Reflections not visible?**
- Move closer to metallic objects (1-2 units)
- Press **Shift+S** to enable SSR
- Increase SSR samples: Press **.** key
- Reduce roughness cutoff: Press **/** key

## Quick Command Reference

```bash
# Showcase scenes
./showcase_scene.sh                    # Interactive scene selector
make phase4-complete GPU_SCENE=pbr_showcase  # Direct build

# Available scenes
SCENE_NAME=cornell_box   # Classic test scene
SCENE_NAME=gpu_demo      # PBR material showcase
SCENE_NAME=pbr_showcase  # Complete material sweep

# Quality levels
make phase4-complete      # All features (15-20 FPS)
make gi-balanced         # GI only (35 FPS)
make cinematic-quality   # Cinematic look

# Documentation
docs/GPU_SHOWCASE_GUIDE.md  # Comprehensive showcase guide
```

---

## Need More?

**Full Documentation:** `docs/GPU_SHOWCASE_GUIDE.md`
**Quick Reference:** `docs/GPU_QUICK_REFERENCE.md`
**Phase 4 Guide:** `docs/GPU_PHASE4_POST_PROCESSING.md`

**Get Started:**
```bash
./showcase_scene.sh
# Choose: 3) PBR Showcase
# Enable: G + Shift+S + E + V + N
# Position: (0, 2, 8) looking at (0, 0.5, 0)
# Press: S to screenshot
```

**Enjoy creating stunning showcase renders!** 🎬✨
