# ASCII Ray Tracer - Interactive Controls

## 🎮 **Controls Overview**

The ASCII ray tracer now supports **full interactive camera controls** entirely in the terminal!

### **Movement Controls**

| Key | Action | Description |
|-----|--------|-------------|
| **W** | Forward | Move camera forward |
| **S** | Backward | Move camera backward |
| **A** | Left | Move camera left |
| **D** | Right | Move camera right |
| **Q** | Up | Move camera up |
| **E** | Down | Move camera down |

### **Look Controls**

| Key | Action | Description |
|-----|--------|-------------|
| **↑** | Look Up | Tilt camera up |
| **↓** | Look Down | Tilt camera down |
| **←** | Look Left | Rotate camera left |
| **→** | Look Right | Rotate camera right |

### **System Controls**

| Key | Action | Description |
|-----|--------|-------------|
| **H** | Help | Toggle help overlay |
| **ESC** | Quit | Exit the ray tracer |

---

## 🚀 **Quick Start**

```bash
# Build and run
make runa

# Or directly
./raytracer_ascii
```

### **Step-by-Step Usage**

1. **Choose Quality Level** (1-3)
   - **1**: Low quality (1 sample) - Fast, very noisy
   - **2**: Medium quality (4 samples) - Balanced ⭐
   - **3**: High quality (16 samples) - Slow, smooth

2. **Read the Help Screen**
   - Press any key to start
   - Controls will be shown

3. **Start Exploring!**
   - Use **WASD** to move around the Cornell Box
   - Use **Arrow Keys** to look around
   - Press **H** anytime for help
   - Press **ESC** to quit

---

## 🎯 **Example Session**

```bash
$ make runa
========================================
   ASCII RAY TRACER - INTERACTIVE
========================================
Initializing...
Terminal size: 80x21

Quality presets:
1. Low (1 sample, fast)
2. Medium (4 samples, balanced)
3. High (16 samples, slow)

Choose quality (1-3): 2

Rendering with 4 samples, max depth 4
Resolution: 80x21

========================================
     ASCII RAY TRACER CONTROLS
========================================
MOVEMENT:
  W/S - Move forward/backward
  A/D - Move left/right
  Q/E - Move up/down
LOOK:
  Arrow Keys - Look around
OTHER:
  H - Toggle this help
  ESC - Quit
========================================

Press any key to start...

[Press any key]

Starting interactive rendering...
Press H for help, ESC to quit

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@%%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

Frame 42 | 0.85s | 0.38 MRays/s | 4 samples | Pos: (2.1, 1.0, 3.5) | H=Help
```

---

## 🕹️ **Control Details**

### **Movement System**

The movement system uses **first-person shooter style controls**:

- **W/S**: Move along the camera's forward vector
- **A/D**: Move along the camera's right vector (strafe)
- **Q/E**: Move straight up/down (world-space vertical)
- All movements are **0.2 units per keypress**

### **Look System**

The look system uses **pitch and yaw rotation**:

- **Arrow Keys**: Rotate camera in 2° increments
- **Pitch** (Up/Down): Clamped to ±89° to prevent gimbal lock
- **Yaw** (Left/Right): Unlimited rotation
- Look direction is **independent of movement direction**

### **Camera Controller**

The camera controller maintains:
- **Position**: Current camera location in 3D space
- **Look-at point**: Calculated from yaw/pitch angles
- **Up vector**: Always points upward (0, 1, 0)
- **Field of view**: 60° (horizontal)

---

## 🎨 **Advanced Techniques**

### **Smooth Navigation**

1. **Orbit the Scene**
   - Hold **←** while pressing **W** to circle around objects
   - Use **↑**/**↓** to change viewing angle

2. **Explore the Corners**
   - Press **Q** to fly up and look down into the box
   - Use **A/D** to strafe along walls

3. **Close-Up Inspection**
   - Press **W** multiple times to get close to objects
   - Use arrow keys to examine details

### **Performance Tips**

- **Lower quality** (1) for smooth exploration
- **Higher quality** (3) for final screenshots
- **Medium quality** (2) for balanced experience

**Performance Reference:**
- 80x21 terminal, Quality 1: ~0.2s/frame
- 80x21 terminal, Quality 2: ~0.8s/frame
- 80x21 terminal, Quality 3: ~3.2s/frame

---

## 🔧 **Technical Implementation**

### **Non-Blocking Input**

The ray tracer uses **non-blocking input** to maintain responsiveness:

**macOS/Linux:**
```cpp
// Disable canonical mode and echo
termios ttystate;
ttystate.c_lflag &= ~(ICANON | ECHO);
tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

// Non-blocking read
fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
```

**Windows:**
```cpp
// Use _kbhit() for non-blocking input
if (_kbhit()) {
    key = _getch();
}
```

### **Cross-Platform Key Handling**

**Arrow Keys:**
- **Windows**: Send 224 followed by key code (72/80/75/77)
- **Unix/Linux/macOS**: Send escape sequence (27, 91, key code)

**Key Codes:**
- **Up**: 72 (Windows) / 65 (Unix)
- **Down**: 80 (Windows) / 66 (Unix)
- **Left**: 75 (Windows) / 68 (Unix)
- **Right**: 77 (Windows) / 67 (Unix)

### **Thread Safety**

- Input processing runs on main thread
- Rendering uses OpenMP parallelization
- Atomic flags control rendering state

---

## 🎮 **Demo Scenarios**

### **Scenario 1: Cornell Box Tour**

```
1. Start at default position
2. Press W 5 times (move forward into box)
3. Press ↓ (look down at floor)
4. Press A 3 times (strafe left)
5. Press ↑ (look up at ceiling)
6. Press S 5 times (back out of box)
```

### **Scenario 2: Orbital Inspection**

```
1. Hold ← (look left)
2. Hold W (move forward)
3. Camera orbits around center
4. Release keys to stop
```

### **Scenario 3: Vertical Exploration**

```
1. Press Q 10 times (fly up)
2. Press ↓ (look down)
3. See entire Cornell Box from above
4. Press E 10 times (fly down)
5. Press ↑ (look up at ceiling)
```

---

## 🐛 **Troubleshooting**

### **Input Not Working**

```bash
# Make sure terminal is in focus
# Click on terminal window before pressing keys

# Check if terminal supports special keys
# Try basic WASD first, then arrow keys
```

### **Terminal Size Issues**

```bash
# macOS/Linux: Check size
stty size

# Resize to at least 40x10
# Recommended: 80x24 or larger
```

### **Performance Issues**

```bash
# Reduce quality level
# Choose 1 instead of 3

# Use smaller terminal
# Resize window to be smaller

# Close other applications
# Free up CPU resources
```

---

## 🚀 **Future Enhancements**

### **Potential Features**

- **Mouse Look**: Use mouse for camera rotation (terminal-dependent)
- **Roll Control**: Z-axis rotation with Z/C keys
- **Speed Modes**: Hold Shift for faster movement
- ** presets**: Save/restore camera positions
- **Screenshot**: Save ASCII art to file
- **Recording**: Save animation as text file

### **Advanced Controls**

```cpp
// Potential future additions
case KEY_SHIFT:  // Faster movement mode
    movement_speed *= 2.0;
    break;
case KEY_SPACE:  // Jump/fly up
    position.y += 1.0;
    break;
case 'r':  // Reset camera
    position = Point3(0, 1, 4);
    lookat = Point3(0, 0.5, 0);
    break;
```

---

## 🎯 **Comparison with SDL2 Version**

| Feature | ASCII Version | SDL2 Version |
|---------|---------------|--------------|
| **Platform** | Any terminal | GUI required |
| **Dependencies** | None | SDL2, SDL2_ttf |
| **Resolution** | Adaptive to terminal | Fixed window size |
| **Rendering** | ASCII art | Full color graphics |
| **Controls** | WASD + Arrows | WASD + Mouse |
| **Performance** | Text I/O bound | GPU accelerated |
| **Style** | Retro aesthetics | Modern graphics |

---

## 📚 **References**

- **First-Person Shooter Controls**: Standard WASD + mouse layout
- **Terminal Programming**: POSIX termios and Windows console API
- **Camera Controllers**: 3D graphics camera math
- **ASCII Art**: Character-based rendering techniques

---

**Enjoy exploring the Cornell Box in pure ASCII!** 🎮✨

The interactive controls make it feel like a retro first-person game running entirely in your terminal!