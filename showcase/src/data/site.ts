/** Project + repo links — tweak copy and URLs as the tracer evolves. */
export const site = {
  projectName: "SIMD Ray Tracer",
  tagline: "CPU-first C++ ray tracer — SIMD, threading, and interactive tooling with receipts.",
  description:
    "Open-source CPU ray tracer: AVX2 + OpenMP, SDL2 interactive mode with progressive/adaptive/wavefront paths, deterministic batch + regression tests, and ASCII terminal output. GPU renderer is WIP until parity and CI catch up.",
  maintainer: "Cade Richard",
  /** Set to your LinkedIn URL to show a footer link; leave empty to hide. */
  maintainerLinkedIn: "",
  email: "hello@example.com",
  githubUrl: "https://github.com/caderichard-debug/ray-tracer",
  repoLabel: "caderichard-debug/ray-tracer",
  readmeUrl: "https://github.com/caderichard-debug/ray-tracer/blob/main/README.md",
  issuesUrl: "https://github.com/caderichard-debug/ray-tracer/issues",
  licenseName: "MIT",
  licenseUrl: "https://github.com/caderichard-debug/ray-tracer/blob/main/LICENSE",
  ogImage: "/og.svg",
} as const;
