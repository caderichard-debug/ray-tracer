# GPU Ray Tracer - Quick Reference Guide

## Keyboard Shortcuts

### Core Controls
| Key | Action | Description |
|-----|--------|-------------|
| **WASD** | Move | Forward/Left/Backward/Right |
| **Arrow Keys** | Move | Up/Down |
| **Mouse** | Look | When captured |
| **Left Click** | Capture/Release Mouse | Toggle mouse control |
| **ESC** | Quit | Exit application |

### Rendering Toggles
| Key | Action | Description |
|-----|--------|-------------|
| **R** | Toggle Reflections | Enable/disable ray-traced reflections |
| **P** | Toggle Phong/PBR | Switch lighting models |
| **L** | Cycle Lights | 1 → 2 → 3 light configurations |

### Global Illumination (Phase 3)
| Key | Action | Range | Default |
|-----|--------|-------|---------|
| **G** | Toggle GI | On/Off | Off |
| **[** | Decrease GI Samples | 1-8 | 4 |
| **]** | Increase GI Samples | 1-8 | 4 |
| **-** | Decrease GI Intensity | 0.0-1.0 | 0.4 |
| **=** | Increase GI Intensity | 0.0-1.0 | 0.4 |

### Screen-Space Reflections (Phase 3.5)
| Key | Action | Range | Default |
|-----|--------|-------|---------|
| **Shift+S** | Toggle SSR | On/Off | Off |
| **,** (comma) | Decrease SSR Samples | 4-32 | 16 |
| **.** (period) | Increase SSR Samples | 4-32 | 16 |
| **/** (slash) | Increase SSR Roughness Cutoff | 0.0-1.0 | 0.7 |

### Environment Mapping (Phase 3.5)
| Key | Action | Description |
|-----|--------|-------------|
| **E** | Toggle Environment | Enable/disable environment mapping |

### Display & Capture
| Key | Action | Description |
|-----|--------|-------------|
| **H** | Toggle Help | Show/hide help overlay |
| **C** | Toggle Controls | Show/hide controls panel |
| **S** | Screenshot | Save PNG to screenshots/ |

---

## Quality Presets

### Fast Performance (60+ FPS)
```bash
make gpu-fast          # Basic PBR, maximum speed
make gi-fast           # Fast GI (2 samples, 0.2 intensity)
```

### Interactive (30-60 FPS)
```bash
make gpu-interactive   # Balanced PBR + tone mapping
make gi-balanced       # Balanced GI (4 samples, 0.4 intensity)
make ssr-fast          # Fast SSR (8 samples)
```

### Production Quality (20-45 FPS)
```bash
make gpu-production    # PBR + soft shadows + AO
make gi-quality        # Quality GI (8 samples, 0.6 intensity)
make ssr-quality       # Quality SSR (24 samples)
make env-quality       # Environment mapping
```

### Maximum Quality (10-20 FPS)
```bash
make gpu-showcase      # All Phase 1-2 features
make phase35-complete  # All Phase 3 + 3.5 features (GI + SSR + Env)
```

---

## Feature Descriptions

### Global Illumination (GI)
**What it does:** Simulates realistic color bleeding and indirect lighting

**When to use:**
- Indoor scenes (Cornell Box)
- Architectural visualization
- When you want realistic color transfer

**How to adjust:**
1. Press **G** to enable
2. Use **[ / ]** for sample count (more = smoother, slower)
3. Use **- / =** for intensity (higher = stronger effect)

**Good starting point:** GI Balanced (4 samples, 0.4 intensity)

### Screen-Space Reflections (SSR)
**What it does:** Real-time ray traced reflections using scene data

**When to use:**
- Scenes with metallic/glossy objects
- Chrome, glass, or reflective materials
- When you want local reflections

**How to adjust:**
1. Press **Shift+S** to enable
2. Use **, / .** for sample count (more = sharper reflections)
3. Use **/** for roughness cutoff (lower = more surfaces reflect)

**Good starting point:** SSR Fast (8 samples)

### Environment Mapping
**What it does:** Procedural sky, sun, and ground plane

**When to use:**
- Outdoor scenes
- When you want sky lighting
- To add environmental depth

**How to adjust:**
1. Press **E** to enable
2. Automatically adjusts based on view direction

**Good starting point:** Environment Quality preset

---

## Performance Optimization Tips

### If Too Slow (Low FPS)

1. **Reduce GI samples:** Press **[** key
2. **Reduce GI intensity:** Press **-** key
3. **Disable SSR:** Press **Shift+S**
4. **Disable Environment:** Press **E** key
5. **Use faster preset:** `make gi-fast` or `make gpu-interactive`

### If Too Fast (Want More Quality)

1. **Increase GI samples:** Press **]** key
2. **Increase GI intensity:** Press **=** key
3. **Enable SSR:** Press **Shift+S**, then **.** key for more samples
4. **Enable Environment:** Press **E** key
5. **Use quality preset:** `make phase35-complete`

### Finding Your Sweet Spot

**Step 1:** Start with `make gi-balanced` (35 FPS typical)

**Step 2:** Press **G** to enable GI

**Step 3:** Adjust quality:
- If 35+ FPS: Add SSR (Shift+S)
- If 25+ FPS: Add Environment (E)
- If < 20 FPS: Reduce samples ([ key)

**Step 4:** Fine-tune:
- Use **- / =** for GI intensity
- Use **, / .** for SSR quality
- Use **/** for SSR roughness

---

## Common Use Cases

### Indoor Scene (Cornell Box)

**Recommended:** `make gi-balanced`

**Why:**
- GI provides most benefit for indoor color bleeding
- SSR adds nice reflections on metal spheres
- Environment less critical (indoor scene)

**Real-time adjustments:**
- Press **G** for GI (essential)
- Press **Shift+S** for SSR (nice to have)
- Skip **E** (environment) unless needed

### Outdoor Scene

**Recommended:** `make env-quality`

**Why:**
- Environment mapping provides biggest benefit
- GI still useful for indirect lighting
- SSR less critical (fewer reflective surfaces)

**Real-time adjustments:**
- Press **E** for environment (essential)
- Press **G** for GI (nice to have)
- Skip **Shift+S** unless many reflective objects

### Product Rendering

**Recommended:** `make phase35-complete`

**Why:**
- Maximum visual quality
- All features work together
- Performance less critical (static shots)

**Real-time adjustments:**
- Enable all features (G, Shift+S, E)
- Adjust quality for final render
- Press **S** for screenshot

### Development/Testing

**Recommended:** `make gpu-fast`

**Why:**
- Maximum performance for quick iteration
- Basic PBR for visual feedback
- 60+ FPS smooth interaction

**Real-time adjustments:**
- Keep features disabled initially
- Enable one at a time to test
- Use minimal samples for speed

---

## Visual Quality Checklist

### Good GI Settings Look Like:
- ✓ Red walls subtly color adjacent surfaces
- ✓ Bright objects illuminate nearby areas
- ✓ No flat ambient appearance
- ✓ Natural color bleeding

### Good SSR Settings Look Like:
- ✓ Metallic surfaces show reflections
- ✓ Chrome acts like mirror
- ✓ Grazing angles are more reflective
- ✓ Rough materials show blur

### Good Environment Settings Look Like:
- ✓ Realistic sky gradient
- ✓ Sun provides highlights
- ✓ Ground adds depth
- ✓ Smooth horizon transition

### Quality Issues Look Like:
- ✗ Grainy/noisy → Increase samples
- ✗ Too bright → Reduce intensity
- ✗ Too dark → Increase intensity
- ✗ No effect → Check feature enabled, reduce roughness cutoff

---

## Screenshot Workflow

### Capturing Comparison Screenshots

1. **Build preset:** `make [preset]`
2. **Run:** `./build/raytracer_interactive_gpu`
3. **Position camera:** Good view of scene
4. **Enable features:** Toggle desired features
5. **Adjust quality:** Fine-tune with keyboard
6. **Capture:** Press **S** key
7. **Find screenshot:** Check `screenshots/` folder

### Automated Comparison Script

```bash
./capture_comparisons.sh
```

This will:
- Build all presets
- Prompt for screenshots
- Organize by phase
- Save to `screenshots/comparisons/`

---

## Troubleshooting

### Black Screen
- Check camera position (use WASD to move)
- Verify scene loaded properly
- Try pressing **R** to toggle reflections

### Low Performance
- Reduce GI samples: Press **[** key
- Reduce GI intensity: Press **-** key
- Disable SSR: Press **Shift+S**
- Disable Environment: Press **E**
- Try faster preset: `make gi-fast`

### No Visible Effect
- Verify feature is enabled (G, Shift+S, E)
- Check intensity/samples are not zero
- Move closer to surfaces to see effect
- Ensure scene has appropriate materials

### Too Bright
- Reduce GI intensity: Press **-** key
- Reduce GI samples: Press **[** key
- Disable environment: Press **E**

### Grainy/Noisy
- Increase GI samples: Press **]** key
- Increase SSR samples: Press **.** key
- Enable environment for smoothing
- Reduce roughness cutoff: Press **/**

---

## Build System Quick Reference

### Build Commands
```bash
make interactive-gpu      # Build GPU interactive (default settings)
make batch-gpu            # Build GPU batch renderer
make clean                # Remove build artifacts
make rebuild              # Clean and rebuild
```

### Run Commands
```bash
make runi-gpu             # Build and run GPU interactive
make run-gpu              # Build and run GPU batch
```

### Feature Flags (Advanced)
```bash
make interactive-gpu \
    ENABLE_PBR=1 \
    ENABLE_GI=1 \
    GI_SAMPLES=6 \
    GI_INTENSITY=0.5 \
    ENABLE_SSR=1 \
    SSR_SAMPLES=12
```

---

## Documentation Links

- **[Comparison Guide](GPU_COMPARISON_GUIDE.md)** - Detailed phase-by-phase comparison
- **[Phase 3 GI](GPU_PHASE3_GI.md)** - Global Illumination implementation
- **[Phase 3.5 Advanced Reflections](GPU_PHASE35_ADVANCED_REFLECTIONS.md)** - SSR and Environment
- **[GPU Renderer Guide](GPU_RENDERER_GUIDE.md)** - Complete GPU documentation

---

## Tips for Best Results

### Camera Positioning
- **GI:** Get close to colorful surfaces
- **SSR:** Angle view to see reflections on metals
- **Environment:** Position to see sky and horizon
- **Overall:** Use corner views for depth

### Material Testing
- **GI:** Test with red/green/blue walls
- **SSR:** Test with chrome/gold/copper spheres
- **Environment:** Test with outdoor scenes
- **All features:** Use Cornell Box with mixed materials

### Quality Settings
- Start low, increase gradually
- Use performance as guide
- Screenshot at max quality for comparison
- Find sweet spot for your GPU

---

**Quick Start:** `make gi-balanced && make runi-gpu`
**Best Quality:** `make phase35-complete && make runi-gpu`
**Maximum Speed:** `make gpu-fast && make runi-gpu`
