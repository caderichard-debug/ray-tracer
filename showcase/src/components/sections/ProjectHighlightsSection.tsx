import { SectionHeader } from "@/components/ui/SectionHeader";

const highlights = [
  {
    title: "Cornell-aligned core",
    body:
      "Interactive SDL, batch PNG, and ASCII modes share scene and material definitions so tuning and comparisons stay honest.",
    foot: "Scene: `src/scene/cornell_box.h`",
  },
  {
    title: "Regression you can run",
    body:
      "`make regression-test` hashes deterministic CPU batch output against golden PPMs — useful when SIMD or shading changes land.",
    foot: "See `tests/regression/`",
  },
  {
    title: "Benchmark trail",
    body:
      "The Makefile drives combinatorial CPU feature runs; summaries and charts live under `benchmark_results/` for before/after deltas.",
    foot: "`make benchmark`",
  },
];

export function ProjectHighlightsSection() {
  return (
    <section
      id="highlights"
      className="scroll-mt-28 space-y-10 border-t border-white/5 bg-surface py-16 sm:py-20"
    >
      <div className="mx-auto max-w-6xl space-y-10 px-4 sm:px-6 lg:px-8">
        <SectionHeader
          eyebrow="Why it stands out"
          title="Proof points from the repo"
          description="Facts you can verify in source and scripts — each pointer maps to a path in this repository."
          align="center"
        />
        <div className="grid gap-6 md:grid-cols-3">
          {highlights.map((item) => (
            <article
              key={item.title}
              className="flex h-full flex-col rounded-2xl border border-white/10 bg-gradient-to-b from-white/[0.04] to-transparent p-6"
            >
              <h3 className="text-base font-semibold text-white">{item.title}</h3>
              <p className="mt-3 flex-1 text-sm leading-relaxed text-slate-300">
                {item.body}
              </p>
              <p className="mt-6 font-mono text-xs text-accent/90">{item.foot}</p>
            </article>
          ))}
        </div>
      </div>
    </section>
  );
}
