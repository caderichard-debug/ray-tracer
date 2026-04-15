# PBR Testing Guide for GPU Ray Tracer

## Quick Start

### Build and Run
```bash
make interactive-gpu
make runi-gpu
```

## Controls

### Lighting Controls
- **P** - Toggle between Phong and PBR lighting modes
- **L** - Cycle through light configurations:
  - 1 light: Single overhead light (fastest)
  - 2 lights: Main + fill light (better balance)
  - 3 lights: Main + fill + rim light (studio quality)

### Other Controls
- **R** - Toggle reflections
- **H** - Toggle help overlay
- **C** - Toggle controls panel
- **WASD/Arrows** - Move camera
- **Mouse** - Look around (when captured)
- **ESC** - Quit

## Testing PBR Features

### 1. Material Quality Test
**Purpose:** Verify PBR materials look better than Phong

**Steps:**
1. Run the ray tracer
2. Press **P** to switch to PBR mode (default is Phong)
3. Observe the metallic spheres (gold and silver)
4. **Expected Results:**
   - Metals should have realistic reflections
   - Roughness variations should be visible
   - Fresnel effects at grazing angles
   - Materials should not look "plastic"

### 2. Light Configuration Test
**Purpose:** Verify multiple light configurations work

**Steps:**
1. Start in PBR mode (press **P** if needed)
2. Press **L** to cycle through light setups
3. Compare 1-light vs 3-light configurations
4. **Expected Results:**
   - 1 light: High contrast, dramatic shadows
   - 2 lights: Softer shadows, better fill
   - 3 lights: Studio-quality lighting with rim highlights

### 3. Backward Compatibility Test
**Purpose:** Ensure existing functionality still works

**Steps:**
1. Start the ray tracer (default Phong mode)
2. Verify scene looks the same as before
3. Press **R** to toggle reflections
4. Move camera around
5. **Expected Results:**
   - Default mode should be unchanged (Phong)
   - All existing controls work
   - Performance should be similar

### 4. Performance Test
**Purpose:** Check performance impact of PBR

**Steps:**
1. Note the FPS in Phong mode (default)
2. Press **P** to switch to PBR mode
3. Compare FPS values
4. Press **L** to add more lights, observe performance
5. **Expected Results:**
   - PBR mode: 15-25% slower than Phong
   - More lights: Progressive slowdown
   - Should still be interactive (30+ FPS at 640x360)

## Visual Quality Checklist

### PBR Mode vs Phong Mode
- [ ] Metals look like metal, not plastic
- [ ] Rough surfaces appear matte
- [ ] Smooth surfaces appear glossy
- [ ] Grazing angles show mirror-like reflections
- [ ] No overbright materials (energy conservation)
- [ ] Color saturation looks natural

### Light Quality
- [ ] Soft shadows with multiple lights
- [ ] Natural fill light (not pitch black shadows)
- [ ] Rim lighting on edges (3-point setup)
- [ ] Balanced exposure (no blown-out highlights)

### Tone Mapping
- [ ] Cinematic contrast (ACES)
- [ ] No blown-out highlights
- [ ] Shadow details preserved
- [ ] Natural color reproduction

## Known Issues

### Performance
- PBR mode is slower than Phong (expected)
- More lights = slower performance
- High resolutions (1920x1080) may be slow

### Visual
- Some materials may look different than Phong (intentional)
- PBR is more physically accurate but may differ from artistic intent

## Troubleshooting

### Build Issues
If you get compile errors:
```bash
make clean
make interactive-gpu
```

### Runtime Issues
If the program crashes:
1. Try lowering resolution in code (WIDTH/HEIGHT constants)
2. Disable reflections (R key)
3. Use Phong mode instead of PBR

### Visual Issues
If materials look wrong:
1. Check you're in PBR mode (P key)
2. Verify controls panel shows correct lighting mode
3. Try different light configurations (L key)

## Next Steps

### Phase 2: Advanced Shadows
- Soft shadows with area light sampling
- Ray-traced ambient occlusion
- Better shadow quality

### Phase 3: Global Illumination
- Path tracing with Monte Carlo integration
- Color bleeding effects
- Progressive rendering

## Performance Benchmarks

### Expected Performance (1280x720)

| Configuration | FPS (Phong) | FPS (PBR) | Overhead |
|--------------|-------------|-----------|----------|
| 1 light      | 75          | 60        | +20%     |
| 2 lights     | 50          | 40        | +20%     |
| 3 lights     | 35          | 28        | +20%     |

*Note: Performance varies by GPU*

## Material Reference

### PBR Parameter Ranges

**Roughness:**
- 0.0 - Mirror (polished metal)
- 0.1 - Polished metal
- 0.3 - Glossy plastic
- 0.5 - Medium roughness
- 0.7 - Matte surface
- 0.9 - Very rough (chalk, drywall)
- 1.0 - Maximum roughness

**Metallic:**
- 0.0 - Dielectric (plastic, wood, stone)
- 0.5 - Semi-metallic (rusty metal)
- 1.0 - Metal (gold, silver, copper)

### Default Material Assignments

The PBR system automatically assigns:
- **Lambertian walls/floors:** roughness=0.8, metallic=0.0
- **Metal spheres:** roughness=0.1, metallic=1.0
- **Fuzzy metal:** roughness=0.4, metallic=1.0
- **Glass:** roughness=0.05, metallic=0.0
- **Checkerboard:** roughness=0.8, metallic=0.0
- **Noise texture:** roughness=0.9, metallic=0.0

## Conclusion

The PBR system provides significant visual quality improvements while maintaining full backward compatibility. Use Phong mode for maximum performance, PBR mode for best quality.

For questions or issues, refer to the main GPU_LIGHTING_IMPROVEMENT_PLAN.md document.
