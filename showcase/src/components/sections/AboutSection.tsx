import { SectionHeader } from "@/components/ui/SectionHeader";

export function AboutSection() {
  return (
    <section
      id="about"
      className="scroll-mt-28 space-y-8 border-t border-white/5 bg-surface py-16 sm:py-20"
    >
      <div className="mx-auto max-w-6xl space-y-10 px-4 sm:px-6 lg:px-8">
        <SectionHeader
          eyebrow="About this project"
          title="Performance with receipts"
          description="A from-scratch C++ tracer that grew from scalar experiments into a SIMD + OpenMP stack with progressive modes, analysis overlays, and a regression harness so refactors do not silently rot pixels."
        />
        <div className="grid gap-6 lg:grid-cols-3">
          <div className="rounded-2xl border border-white/10 bg-white/[0.03] p-6 transition hover:border-accent/30">
            <h3 className="text-lg font-semibold text-white">CPU-first contract</h3>
            <p className="mt-3 text-sm leading-relaxed text-slate-400">
              Interactive SDL mode is the hero surface: translucent controls, live
              histograms, denoise blending, and capture timing that respects the
              render pass.
            </p>
          </div>
          <div className="rounded-2xl border border-white/10 bg-white/[0.03] p-6 transition hover:border-accent/30">
            <h3 className="text-lg font-semibold text-white">ASCII + batch siblings</h3>
            <p className="mt-3 text-sm leading-relaxed text-slate-400">
              Terminal output and offline batch stay aligned on scene definitions so
              experiments port cleanly between headless and GUI workflows.
            </p>
          </div>
          <div className="rounded-2xl border border-white/10 bg-white/[0.03] p-6 transition hover:border-accent/30">
            <h3 className="text-lg font-semibold text-white">GPU as R&D lane</h3>
            <p className="mt-3 text-sm leading-relaxed text-slate-400">
              OpenGL path explores effects and presentation, but docs keep expectations
              honest until parity, CI coverage, and benchmarks catch up.
            </p>
          </div>
        </div>
      </div>
    </section>
  );
}
