# Getting Started with GPU Ray Tracing

## First Time Setup

### 1. Install Dependencies

**macOS:**
```bash
brew install gcc sdl2 sdl2_ttf glew
```

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev libglew-dev
```

**Windows (MSYS2):**
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-GLEW
```

### 2. Verify OpenGL Support

```bash
# Check OpenGL version (should be 2.0+ for GLSL 1.20)
glxinfo | grep "OpenGL version"  # Linux
# On macOS, OpenGL 2.0+ is standard
```

**Note:** Intel i7 integrated GPUs support OpenGL 2.0+ (GLSL 1.20)

## Quick Start (5 Minutes)

### Step 1: Build and Run

```bash
# Build the balanced GPU preset
make gi-balanced

# Run the GPU ray tracer
./build/raytracer_interactive_gpu
```

### Step 2: Basic Controls

**Movement:**
- **WASD** - Move forward/left/backward/right
- **Arrow Keys** - Move up/down
- **Mouse** - Look around (click to capture)

**Rendering:**
- **H** - Show help overlay
- **S** - Save screenshot
- **ESC** - Quit

### Step 3: Enable Features

Press these keys to see improvements:

1. **G** - Enable Global Illumination (color bleeding)
2. **Shift+S** - Enable Screen-Space Reflections
3. **E** - Enable Environment Mapping

**Quality Adjustment:**
- **[ / ]** - Adjust GI samples
- **- / =** - Adjust GI intensity
- **, / .** - Adjust SSR samples
- **/** - Adjust SSR roughness

## Quality Presets Guide

### For Development/Testing
```bash
make gpu-fast          # Maximum speed (75+ FPS)
./build/raytracer_interactive_gpu
```
- Basic PBR rendering
- No advanced features
- Best for quick iteration

### For Interactive Exploration
```bash
make gi-balanced       # Balanced quality/speed (35+ FPS)
./build/raytracer_interactive_gpu
```
- Global Illumination enabled
- Good color bleeding
- Smooth performance

### For Maximum Quality
```bash
make phase35-complete  # All features (20 FPS)
./build/raytracer_interactive_gpu
```
- GI + SSR + Environment
- Cinematic quality
- Best for screenshots

## Understanding the Features

### Global Illumination (GI)

**What it does:**
- Simulates color bleeding between surfaces
- Adds realistic indirect lighting
- Makes scenes look more natural

**When to use:**
- Indoor scenes (Cornell Box)
- Architectural visualization
- When you want realistic lighting

**How to control:**
- Press **G** to toggle on/off
- Press **[ / ]** to adjust samples (1-8)
- Press **- / =** to adjust intensity (0.0-1.0)

**Good settings:**
- Fast: 2 samples, 0.2 intensity
- Balanced: 4 samples, 0.4 intensity
- Quality: 8 samples, 0.6 intensity

### Screen-Space Reflections (SSR)

**What it does:**
- Real-time ray traced reflections
- Metallic surfaces show reflections
- Chrome acts like mirror

**When to use:**
- Scenes with metallic objects
- Chrome, glass, or glossy materials
- When you want local reflections

**How to control:**
- Press **Shift+S** to toggle on/off
- Press **, / .** to adjust samples (4-32)
- Press **/** to adjust roughness (0.0-1.0)

**Good settings:**
- Fast: 8 samples
- Quality: 24 samples
- Roughness: 0.7 (default)

### Environment Mapping

**What it does:**
- Procedural sky gradient
- Realistic sun disk
- Ground plane with horizon

**When to use:**
- Outdoor scenes
- When you want sky lighting
- To add environmental depth

**How to control:**
- Press **E** to toggle on/off
- Automatically adjusts based on view

## Screenshot Workflow

### Quick Screenshot

1. Run ray tracer: `./build/raytracer_interactive_gpu`
2. Navigate to good view (WASD + mouse)
3. Enable features (G, Shift+S, E)
4. Press **S** to capture
5. Find in `screenshots/` folder

### Comparison Screenshots

Use the automated script:

```bash
./capture_comparisons.sh
```

This will:
- Build all quality presets
- Launch ray tracer for each
- Prompt you to capture screenshots
- Organize by phase and quality

### Manual Comparison

**Step 1:** Build baseline
```bash
make gpu-fast
./build/raytracer_interactive_gpu
# Press S to save baseline screenshot
```

**Step 2:** Build with GI
```bash
make gi-balanced
./build/raytracer_interactive_gpu
# Press G to enable GI
# Press S to save GI screenshot
```

**Step 3:** Build with all features
```bash
make phase35-complete
./build/raytracer_interactive_gpu
# Press G, Shift+S, E to enable all features
# Press S to save complete screenshot
```

**Step 4:** Compare screenshots in `screenshots/` folder

## Performance Optimization

### If Too Slow (< 20 FPS)

**Reduce GI:**
- Press **[** to reduce samples
- Press **-** to reduce intensity

**Disable SSR:**
- Press **Shift+S** to disable

**Disable Environment:**
- Press **E** to disable

**Use Faster Preset:**
```bash
make gi-fast
```

### If Too Fast (> 60 FPS)

**Increase Quality:**
- Press **]** to increase GI samples
- Press **=** to increase GI intensity
- Press **Shift+S** to enable SSR
- Press **.** to increase SSR samples
- Press **E** to enable environment

**Use Quality Preset:**
```bash
make phase35-complete
```

## Common Issues

### Black Screen

**Solution:**
- Check camera position (use WASD to move)
- Try pressing **R** to toggle reflections
- Verify scene loaded properly

### No Visible Effect

**Solution:**
- Verify feature enabled (G, Shift+S, E)
- Check intensity/samples not zero
- Move closer to surfaces
- Ensure scene has appropriate materials

### Low Performance

**Solution:**
- Reduce GI samples: Press **[**
- Reduce GI intensity: Press **-**
- Disable SSR: Press **Shift+S**
- Try faster preset: `make gi-fast`

### Grainy/Noisy

**Solution:**
- Increase GI samples: Press **]**
- Increase SSR samples: Press **.**
- Enable environment for smoothing
- Reduce roughness: Press **/**

## Next Steps

### Learn More

- **[Quick Reference Guide](GPU_QUICK_REFERENCE.md)** - All keyboard shortcuts
- **[Comparison Guide](GPU_COMPARISON_GUIDE.md)** - Visual quality progression
- **[Phase 3 GI](GPU_PHASE3_GI.md)** - Global Illumination details
- **[Phase 3.5 Reflections](GPU_PHASE35_ADVANCED_REFLECTIONS.md)** - SSR and environment

### Capture Comparisons

```bash
# Automated screenshot capture
./capture_comparisons.sh

# Manual capture for specific features
make gi-balanced && ./build/raytracer_interactive_gpu
# Enable features and press S to capture
```

### Experiment with Settings

**Indoor Scene (Cornell Box):**
```bash
make gi-balanced
# Press G for GI (most benefit)
# Press Shift+S for SSR (nice to have)
```

**Outdoor Scene:**
```bash
make env-quality
# Press E for environment (most benefit)
# Press G for GI (nice to have)
```

**Maximum Quality:**
```bash
make phase35-complete
# Press G, Shift+S, E for all features
# Adjust quality with keyboard
```

## Build System

### Common Commands

```bash
make interactive-gpu      # Build GPU interactive
make runi-gpu             # Build and run
make clean                # Clean build
make rebuild              # Clean and rebuild
```

### Quality Presets

```bash
make gpu-fast             # Basic PBR (75 FPS)
make gpu-interactive      # Balanced PBR (60 FPS)
make gpu-production       # PBR + soft shadows (45 FPS)
make gi-fast              # Fast GI (55 FPS)
make gi-balanced          # Balanced GI (35 FPS)
make gi-quality           # Quality GI (20 FPS)
make ssr-fast             # Fast SSR (40 FPS)
make ssr-quality          # Quality SSR (25 FPS)
make env-quality          # Environment (45 FPS)
make phase35-complete     # All features (20 FPS)
```

### Custom Builds

```bash
make interactive-gpu \
    ENABLE_PBR=1 \
    ENABLE_GI=1 \
    GI_SAMPLES=6 \
    GI_INTENSITY=0.5 \
    ENABLE_SSR=1 \
    SSR_SAMPLES=12
```

## Tips for Best Results

### Camera Positioning
- **GI testing:** Get close to colorful surfaces
- **SSR testing:** Angle to see reflections on metals
- **Environment:** Position to see sky and horizon
- **Overall:** Use corner views for depth

### Quality Settings
- Start low, increase gradually
- Use performance as guide
- Screenshot at max quality
- Find sweet spot for your GPU

### Screenshot Tips
- Use consistent camera positions
- Capture same view across presets
- Enable features incrementally
- Note settings for comparison

## Resources

### Documentation
- [GPU Comparison Guide](GPU_COMPARISON_GUIDE.md) - Visual quality comparison
- [GPU Quick Reference](GPU_QUICK_REFERENCE.md) - Keyboard shortcuts
- [Phase 3 GI Guide](GPU_PHASE3_GI.md) - Global Illumination
- [Phase 3.5 Guide](GPU_PHASE35_ADVANCED_REFLECTIONS.md) - Advanced reflections

### Tools
- `capture_comparisons.sh` - Automated screenshot capture
- `Makefile` - Build system with quality presets

### Community
- Report issues on GitHub
- Share screenshots and settings
- Contribute improvements

---

**Ready to start?** Run: `make gi-balanced && make runi-gpu`

**Need help?** Press **H** in the application for help overlay
