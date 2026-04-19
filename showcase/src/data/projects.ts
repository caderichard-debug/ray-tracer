import type { Project } from "@/types/project";

const rawBase =
  "https://raw.githubusercontent.com/caderichard-debug/ray-tracer/main";

export const projects: Project[] = [
  {
    id: "cpu-interactive",
    title: "CPU interactive renderer",
    tagline: "SDL2 real-time mode with live analysis, denoise, and capture.",
    category: "Rendering",
    stack: ["C++", "AVX2", "OpenMP", "SDL2"],
    problem:
      "Interactive ray tracing needs stable frame pacing, coherent SIMD work, and trustworthy tooling overlays without polluting captures.",
    solution:
      "A multi-strategy CPU path (progressive, adaptive, wavefront) with a translucent settings surface, histogram + analysis modes, and render-target screenshots taken before UI composite.",
    results: [
      "Stacked optimizations yield ~40× wall-clock reduction vs early scalar baselines on representative Cornell scenes (see repo benchmarks).",
      "Separable bilateral + variance-guided post blend with a 0–100% denoise slider in-panel.",
      "Camera hot reload via file or env with F5 / optional polling.",
    ],
    imageSrc: `${rawBase}/readme-examples/cornell-box-cpu-scanline.png`,
    imageAlt:
      "Cornell box scene rendered on the CPU with scanline order at 640 by 360 pixels",
    featured: true,
    status: "stable",
  },
  {
    id: "batch-regression",
    title: "Batch + regression harness",
    tagline: "Deterministic PPM checks for CPU batch output.",
    category: "Tooling",
    stack: ["C++", "Make", "SHA-256"],
    problem:
      "Renderer refactors silently shift pixels; you need a fast gate that catches drift without flaky GPU drivers.",
    solution:
      "`make regression-test` compares deterministic CPU batch output to golden hashes with documented flags for reproducibility.",
    results: [
      "CI-friendly deterministic path (documented OpenMP-off batch options in repo).",
      "Cornell family scene stays aligned across interactive and batch modes.",
    ],
    imageSrc: `${rawBase}/readme-examples/cornell-box-cpu-morton.png`,
    imageAlt:
      "Cornell box CPU batch render using Morton Z-order traversal at 640 by 360",
    featured: true,
    status: "stable",
  },
  {
    id: "ascii-mode",
    title: "ASCII terminal renderer",
    tagline: "Cross-platform retro output with animated camera orbits.",
    category: "Experience",
    stack: ["C++", "Terminal"],
    problem:
      "Demos should run anywhere—even SSH—without SDL while still exercising the shared shading core.",
    solution:
      "Terminal mode reuses the CPU renderer stack, mapping luminance to glyphs with adaptive terminal sizing.",
    results: [
      "Ships as a first-class `make ascii` target for workshops and quick smoke tests.",
      "Keeps material and scene definitions consistent with GUI modes.",
    ],
    imageSrc: `${rawBase}/readme-examples/cornell-box-cpu-batch-800.png`,
    imageAlt:
      "High-resolution Cornell box CPU batch render at 1920 by 1080 with 256 samples per pixel",
    featured: true,
    status: "stable",
  },
  {
    id: "gpu-wip",
    title: "GPU OpenGL path",
    tagline: "Experimental throughput mode — not the primary product surface.",
    category: "Rendering",
    stack: ["C++", "OpenGL", "GLSL"],
    problem:
      "Need a playground for real-time shading experiments separate from the CPU regression story.",
    solution:
      "Standalone SDL + OpenGL interactive target with a growing effect stack, clearly labeled WIP in docs.",
    results: [
      "Useful for prototyping post chains and scene presentation before parity work.",
      "No marketing GPU-vs-CPU speed claims until benchmarks are stable in CI.",
    ],
    imageSrc: `${rawBase}/readme-examples/cornell-box-cpu-scanline.png`,
    imageAlt: "Placeholder visual — GPU gallery assets ship separately while path is WIP",
    featured: false,
    status: "wip",
  },
];

export const allStacks = Array.from(
  new Set(projects.flatMap((p) => p.stack)),
).sort((a, b) => a.localeCompare(b));

export const categories: Array<Project["category"] | "All"> = [
  "All",
  "Rendering",
  "Tooling",
  "Experience",
];
