# ASCII Ray Tracer - Agent Context

## Project Overview
Retro-style ray tracer that renders 3D scenes entirely in ASCII art within the terminal. No GUI dependencies - pure text-based rendering with animated camera orbits. Focuses on aesthetic appeal and cross-platform compatibility rather than performance.

**Repository context (2026-04):** The product surface is **CPU-first** (interactive + batch). ASCII remains a **supported secondary** mode for demos and education; GPU work is **WIP** (see `claude-raytracer-gpu.md`).

## Current Development Status

### Completed Features
- ✅ Pure terminal rendering (no GUI required)
- ✅ Real-time ASCII art output with brightness mapping
- ✅ Automatic camera animation (orbits scene)
- ✅ Quality presets (1-3: 1, 4, 16 samples)
- ✅ Performance stats (FPS, MRays/sec, frame time)
- ✅ Adaptive terminal sizing (cross-platform)
- ✅ Cross-platform support (macOS/Linux/Windows)
- ✅ Optimized animation timing (75 frames at 100ms delay)
- ✅ Grayscale conversion using luminance formula
- ✅ Brightness-to-ASCII character mapping

### Design Philosophy
**Retro aesthetic over performance:**
- Terminal-based rendering (nostalgic appeal)
- Animated camera orbits (cinematic quality)
- ASCII character art (visual style)
- Cross-platform compatibility (works everywhere)
- Educational value (shows ray tracing fundamentals)

## Architecture

### Core Files

**Main ASCII Application:**
- **src/main_ascii.cpp** - Standalone ASCII terminal ray tracer
  - Terminal size detection (cross-platform)
  - Scene setup (Cornell Box)
  - Animated camera orbit logic
  - ASCII conversion and output
  - Performance statistics
  - Frame timing and animation control

**Shared Components:**
- **src/renderer/renderer.cpp** - Core ray tracing engine
  - Same engine as CPU interactive mode
  - Phong shading with shadows
  - Material system (Lambertian, Metal)
  - Multi-threaded with OpenMP

**Scene and Materials:**
- **src/scene/cornell_box.h** - Standard test scene
- **src/material/material.h** - Material definitions
- **src/camera/camera.h** - Camera model

## ASCII Rendering System

### Character Mapping
**Brightness to ASCII conversion:**
```cpp
const char ASCII_CHARS[] = "  .:-=+*#%@";
// Darkest to brightest mapping
```

**Luminance formula:**
```cpp
float gray = 0.299 * R + 0.587 * G + 0.114 * B;
// Human perception weighting
```

### Rendering Pipeline
1. **Terminal Detection**: Detect terminal size (cross-platform APIs)
2. **Scene Setup**: Load Cornell Box scene
3. **Camera Orbit**: Calculate camera position for current frame
4. **Ray Tracing**: Render each pixel of virtual image
5. **ASCII Conversion**: Convert RGB to grayscale, then to ASCII
6. **Output**: Print characters to terminal
7. **Animation**: Update camera position, clear, repeat

### Terminal Size Detection
```cpp
// macOS/Linux
struct winsize w;
ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
int width = w.ws_col;
int height = w.ws_row;

// Windows
CONSOLE_SCREEN_BUFFER_INFO csbi;
GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
int width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
int height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
```

## Performance Characteristics

### Terminal Size Performance
| Terminal Size | Quality | Frame Time | Throughput |
|---------------|---------|------------|------------|
| 80x24 | Low (1 sample) | ~0.2s | ~0.01 MRays/s |
| 80x24 | Medium (4 samples) | ~0.8s | ~0.38 MRays/s |
| 80x24 | High (16 samples) | ~3.2s | ~0.10 MRays/s |

### Performance Bottleneck
- **Memory-bound**: Terminal I/O is the limiting factor
- **Not compute-bound**: Ray tracing is fast compared to text output
- **Optimization focus**: Reduce terminal updates, optimize ASCII conversion

## Animation System

### Camera Orbit
```cpp
float angle = frame * 0.05f;  // 5 degrees per frame
float radius = 8.0f;
Vec3 camera_pos = Vec3(
    radius * cos(angle),
    1.0f,  // Fixed height
    radius * sin(angle)
);
Vec3 camera_target = Vec3(0, 0, 0);  // Look at origin
```

### Animation Timing
- **Total frames**: 75 frames
- **Frame delay**: 100ms between frames
- **Total duration**: ~7.5 seconds for full orbit
- **Frame timing**: Display frame time, MRays/sec, sample count

### Quality Presets
```cpp
// Quality 1: Low (1 sample) - Very fast, very noisy
// Quality 2: Medium (4 samples) - Balanced (recommended)
// Quality 3: High (16 samples) - Slow, smooth result
```

## Build System

### Build Targets
```bash
make ascii    # Build ASCII terminal ray tracer
make runa     # Run ASCII terminal ray tracer
```

### Compiler Flags
```makefile
CXXFLAGS = -std=c++17 -O3 -march=native -mavx2 -mfma -ffast-math
INCLUDES = -Isrc -Iexternal
LDFLAGS = -lomp
```

### Dependencies
- **OpenMP**: For multi-threaded ray tracing
- **Standard C++ libraries**: No external GUI dependencies
- **Platform-specific**: Terminal size detection APIs

## Controls and Usage

### Running
```bash
make runa  # Build and run ASCII ray tracer
```

### Usage Flow
1. Choose quality level (1-3)
2. Watch ASCII art animation
3. Press Ctrl+C to stop

### Quality Levels
- **1**: Low quality (1 sample) - very fast, very noisy
- **2**: Medium quality (4 samples) - balanced (recommended)
- **3**: High quality (16 samples) - slow, smooth result

## Platform Support

### Tested Platforms
- **macOS**: ✅ Full support with terminal size detection
- **Linux**: ✅ Full support with terminal size detection
- **Windows**: ✅ Basic support (may need manual terminal size)

### Terminal Compatibility
**Tested terminals:**
- **Terminal.app** (macOS) ✅
- **iTerm2** (macOS) ✅
- **GNOME Terminal** (Linux) ✅
- **Windows Terminal** ✅
- **tmux** ✅

**Requirements:**
- Clear screen (`clear` or `cls`)
- Cursor positioning (ANSI escape codes or Windows API)
- Standard output
- Monospace font (for best results)

## Known Issues and Constraints

### Performance Limitations
- **Terminal I/O bound**: Text output is slower than ray tracing
- **No GPU acceleration**: Pure CPU rendering
- **Limited resolution**: Terminal size constraints
- **No interactive controls**: Pre-programmed animation only

### Platform Constraints
- **Windows**: Terminal size detection may not work on all terminals
- **Small terminals**: < 40x10 may not display properly
- **Font issues**: Non-monospace fonts misalign ASCII

### Display Issues
- **Aspect ratio**: Terminal characters aren't square
- **Color support**: Current implementation is grayscale only
- **Scrolling**: Long animations may cause terminal scroll

## Development Guidelines

### Adding ASCII Features
1. **Test terminal compatibility**: Verify on multiple terminals
2. **Consider font differences**: Not all monospace fonts are equal
3. **Optimize I/O**: Reduce terminal updates where possible
4. **Maintain cross-platform**: Use terminal size detection APIs
5. **Preserve aesthetic**: Keep retro ASCII art feel

### ASCII Art Techniques
**Character sets:**
```cpp
// Simple: " .:-=+*#%@"
// Detailed: " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$"
// Binary: " 01"
// Braille: Unicode block characters (4 rows per character)
```

**Enhancement ideas:**
- Color ASCII using ANSI escape codes
- Multiple character sets for different detail levels
- Animated camera paths (figure-8, spiral)
- Interactive camera control (if possible in terminal)
- Recording/playback of ASCII animations

## Future Enhancements

### Potential Features
**Interactive Controls:**
- WASD camera movement in terminal
- Arrow keys for quality adjustment
- Space to pause/resume animation
- Q to quit

**Advanced ASCII:**
- Color ASCII art (ANSI color codes)
- Unicode block characters (Braille patterns)
- Dithering patterns for better gradients
- Multiple character sets for detail levels

**Animation Features:**
- Multiple camera paths (orbit, spiral, figure-8)
- Variable animation speed
- Scene transitions
- Recording ASCII output to file

### Cross-Platform Improvements
**Terminal detection:**
- Better Windows terminal size detection
- Fallback to manual size specification
- Terminal capability detection
- Font metrics adjustment

## Troubleshooting

### Common Issues

**Terminal size detection fails:**
```bash
# macOS/Linux: Check terminal size
stty size

# Manual size specification (if needed)
export TERMINAL_WIDTH=80
export TERMINAL_HEIGHT=24
```

**Performance too slow:**
- Reduce quality level (choose 1 instead of 3)
- Use smaller terminal window
- Close other applications
- Reduce sample count in code

**Display issues:**
- Use monospace font
- Increase terminal size to at least 40x10
- Try different terminal emulator
- Check for special character support

**Platform-specific:**
- **macOS**: Terminal.app and iTerm2 work best
- **Linux**: GNOME Terminal, xfce-terminal recommended
- **Windows**: Windows Terminal recommended over cmd.exe

## Coordination with Other Agents

### CPU Agent
- **Shared components**: Both use renderer.cpp
- **Different goals**: ASCII for aesthetics, CPU for performance
- **Scene compatibility**: Both use Cornell Box
- **No performance competition**: Different use cases

### GPU Agent
- **No direct interaction**: Different rendering paradigms
- **Scene sharing**: Both use Cornell Box for consistency
- **Learning value**: ASCII shows ray tracing fundamentals

### Manager Agent
- **Build system**: Shared Makefile, different targets
- **Documentation**: Separate ASCII documentation
- **Testing**: Independent testing requirements

## Important Reminders

### DO:
- ✅ Always test on multiple terminals
- ✅ Maintain cross-platform compatibility
- ✅ Optimize terminal I/O (bottleneck)
- ✅ Keep ASCII art aesthetic
- ✅ Test quality presets work correctly
- ✅ Handle terminal size gracefully

### DON'T:
- ❌ Break cross-platform compatibility
- ❌ Ignore terminal size constraints
- ❌ Add GUI dependencies
- ❌ Optimize ray tracing at expense of I/O
- ❌ Use platform-specific APIs without fallbacks
- ❌ Assume square terminal cells

## Success Metrics

### Performance Goals
- **Startup time**: < 1 second to first frame
- **Frame rate**: Smooth animation (10 FPS minimum)
- **Responsiveness**: Immediate Ctrl+C response
- **Memory usage**: < 100MB

### Quality Goals
- **Visual appeal**: Recognizable 3D scene
- **Animation smooth**: No jumps or stutters
- **Character mapping**: Clear brightness gradient
- **Cross-platform**: Works on macOS, Linux, Windows

### User Experience Goals
- **Retro aesthetic**: Authentic ASCII art feel
- **Ease of use**: Simple 1-3 quality selection
- **Reliability**: No crashes or hangs
- **Documentation**: Clear usage instructions

## Fun Features

### ASCII Art Recording
```bash
# Record ASCII animation to text file
./raytracer_ascii > ascii_animation.txt

# Play back with timing
sleep 0.8s < ascii_animation.txt
```

### Custom Character Sets
```cpp
// More detailed: " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$"
// Simple: " .:-=+*#%@"
// Binary: " 01"
// Inverted: "@%#*+=-:.  "
```

### Animation Timing
- **Fast orbit**: 50ms delay (150 frames)
- **Normal orbit**: 100ms delay (75 frames)
- **Slow orbit**: 200ms delay (37 frames)
- **Single frame**: Infinite delay (manual advancement)

---

**Agent Role**: ASCII renderer development and enhancement
**Primary Focus**: Aesthetic appeal and cross-platform compatibility
**Coordination**: Work with CPU agent for shared renderer, manager for overall architecture
