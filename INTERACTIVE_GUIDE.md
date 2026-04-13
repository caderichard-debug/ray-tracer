# Real-Time Interactive Ray Tracer

## Overview
The interactive ray tracer provides real-time rendering with camera movement and adjustable quality settings.

## Building and Running

```bash
# Build the interactive version
make interactive

# Run the interactive ray tracer
make runi

# Or run directly
./raytracer_interactive
```

## Controls

### Movement
- **W/S** - Move forward/backward
- **A/D** - Move left/right
- **Arrow Up/Down** - Move up/down
- **Mouse** - Look around (when mouse is captured)
- **Left Click** - Capture/release mouse

### Quality Control (Capped at Level 3 for Real-Time Performance)
- **1** - Preview (Ultra Fast) - 320x180, 1 sample, depth 1
- **2** - Low (Fast) - 640x360, 1 sample, depth 3
- **3** - Medium - 800x450, 4 samples, depth 3

### Other
- **H** - Toggle help overlay (shows all controls in-window)
- **Space** - Pause/resume rendering
- **ESC** - Quit

## Performance Tips

1. **Start at quality level 2** for smooth exploration
2. **Switch to level 1** when moving the camera quickly
3. **Use higher levels (4-6)** when you find a good composition
4. **Press Space** to pause and save CPU resources

## Quality Levels Explained

| Level | Name | Resolution | Samples | Max Depth | Use Case |
|-------|------|------------|---------|-----------|----------|
| 1 | Preview | 320x180 | 1 | 1 | Fast camera movement |
| 2 | Low | 640x360 | 1 | 3 | Real-time exploration |
| 3 | Medium | 800x450 | 4 | 3 | Balanced quality/speed |
| 4 | High | 800x450 | 16 | 5 | Good quality final render |
| 5 | Ultra | 1200x675 | 32 | 5 | High quality screenshots |
| 6 | Maximum | 1920x1080 | 64 | 8 | Maximum quality output |

## Technical Details

- **Rendering**: Multi-threaded with OpenMP (8 threads)
- **Display**: SDL2 window with hardware acceleration
- **Frame timing**: Real-time FPS display
- **Camera**: First-person style with smooth movement

## Troubleshooting

**Problem**: Low frame rate
- **Solution**: Lower quality level (press 1 or 2)

**Problem**: Window is too small/large
- **Solution**: Change quality level to resize window

**Problem**: Mouse look doesn't work
- **Solution**: Click in the window to capture the mouse

**Problem**: Want to save an image
- **Solution**: Use the batch version: `./raytracer -w 1920 -s 64 -o output`

## Example Workflow

1. Start the interactive ray tracer: `make runi`
2. Begin at quality level 2 (press 2)
3. Explore the scene using WASD + mouse
4. Find an interesting angle
5. Increase to quality level 4 or 5 (press 4 or 5)
6. Press Space to pause rendering
7. Use batch ray tracer for final high-quality output:
   ```bash
   ./raytracer -w 1920 -s 64 -o my_scene
   ```
