import { SectionHeader } from "@/components/ui/SectionHeader";

const groups = [
  {
    title: "Languages & runtime",
    items: ["C++17/20 patterns", "AVX2 intrinsics", "OpenMP scheduling", "SDL2 event loop"],
  },
  {
    title: "Rendering",
    items: [
      "Phong + shadows + reflections",
      "Progressive / adaptive / wavefront",
      "Post denoise + histogram",
      "Material system (diffuse/metal/glass)",
    ],
  },
  {
    title: "Tooling",
    items: ["GNU Make matrix", "Deterministic batch", "Benchmark harness", "GitHub Pages + Vercel"],
  },
];

export function SkillsSection() {
  return (
    <section
      id="skills"
      className="scroll-mt-28 space-y-10 border-t border-white/5 bg-surface-elevated py-16 sm:py-20"
    >
      <div className="mx-auto max-w-6xl space-y-10 px-4 sm:px-6 lg:px-8">
        <SectionHeader
          eyebrow="Implementation"
          title="What this codebase is built from"
          description="Concrete pieces you will find in the tree: the Makefile feature matrix, shared renderer core, SDL interactive shell, and docs that keep GPU expectations separate from the CPU product story."
        />
        <div className="grid gap-6 md:grid-cols-3">
          {groups.map((group) => (
            <div
              key={group.title}
              className="rounded-2xl border border-white/10 bg-surface p-6 shadow-inner shadow-black/40"
            >
              <h3 className="text-sm font-semibold text-white">{group.title}</h3>
              <ul className="mt-4 space-y-3 text-sm text-slate-300">
                {group.items.map((item) => (
                  <li key={item} className="flex gap-2">
                    <span className="mt-1 h-1.5 w-1.5 rounded-full bg-accent" aria-hidden />
                    <span>{item}</span>
                  </li>
                ))}
              </ul>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
