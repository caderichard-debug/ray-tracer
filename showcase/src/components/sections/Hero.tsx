import { site } from "@/data/site";
import { LinkButton } from "@/components/ui/Button";

export function Hero() {
  return (
    <section className="relative overflow-hidden border-b border-white/5">
      <div className="pointer-events-none absolute inset-0 bg-[radial-gradient(circle_at_top,_rgba(45,212,191,0.18),transparent_45%),radial-gradient(circle_at_20%_20%,rgba(59,130,246,0.12),transparent_35%)]" />
      <div className="relative mx-auto flex max-w-6xl flex-col gap-10 px-4 py-16 sm:px-6 sm:py-20 lg:flex-row lg:items-center lg:justify-between lg:px-8 lg:py-24">
        <div className="max-w-2xl space-y-6">
          <p className="text-xs font-semibold uppercase tracking-[0.3em] text-accent">
            Open source · C++ · CPU path
          </p>
          <div className="space-y-4">
            <h1 className="text-balance text-4xl font-semibold tracking-tight text-white sm:text-5xl lg:text-6xl">
              {site.projectName}
            </h1>
            <p className="text-pretty text-lg text-slate-300 sm:text-xl">
              {site.tagline}
            </p>
            <p className="text-pretty text-base text-slate-400">{site.description}</p>
          </div>
          <div className="flex flex-wrap gap-3">
            <LinkButton href="#showcase" variant="primary">
              Explore surfaces
            </LinkButton>
            <LinkButton href={site.readmeUrl} variant="outline" target="_blank" rel="noreferrer">
              README and quick start
            </LinkButton>
            <LinkButton href={site.githubUrl} variant="ghost" target="_blank" rel="noreferrer">
              Source on GitHub
            </LinkButton>
          </div>
          <dl className="grid gap-4 sm:grid-cols-3">
            <div className="rounded-2xl border border-white/10 bg-white/5 p-4">
              <dt className="text-xs uppercase tracking-wide text-slate-500">
                SIMD + threading
              </dt>
              <dd className="mt-2 text-2xl font-semibold text-white">AVX2 + OpenMP</dd>
              <dd className="text-xs text-slate-400">Primary interactive path</dd>
            </div>
            <div className="rounded-2xl border border-white/10 bg-white/5 p-4">
              <dt className="text-xs uppercase tracking-wide text-slate-500">
                Quality gate
              </dt>
              <dd className="mt-2 text-2xl font-semibold text-white">Regression batch</dd>
              <dd className="text-xs text-slate-400">Deterministic PPM hashes</dd>
            </div>
            <div className="rounded-2xl border border-white/10 bg-white/5 p-4">
              <dt className="text-xs uppercase tracking-wide text-slate-500">
                GPU track
              </dt>
              <dd className="mt-2 text-2xl font-semibold text-amber-200">WIP</dd>
              <dd className="text-xs text-slate-400">OpenGL experiments</dd>
            </div>
          </dl>
        </div>
        <div className="relative w-full max-w-xl rounded-3xl border border-white/10 bg-gradient-to-b from-white/10 to-white/[0.02] p-[1px] shadow-[0_40px_120px_-60px_rgba(45,212,191,0.8)]">
          <div className="rounded-3xl bg-surface-elevated/90 p-6 backdrop-blur">
            <p className="text-sm font-semibold text-white">What you get in-tree</p>
            <p className="mt-2 text-sm text-slate-400">
              One Cornell-aligned core, multiple front ends: real-time SDL controls,
              offline PNG batch, terminal ASCII, and a separate GPU playground that stays
              clearly labeled experimental.
            </p>
            <div className="mt-6 space-y-3 text-sm text-slate-300">
              <div className="flex items-center justify-between rounded-xl bg-white/5 px-3 py-2 ring-1 ring-white/10">
                <span>Progressive + adaptive + wavefront</span>
                <span className="text-accent">CPU interactive</span>
              </div>
              <div className="flex items-center justify-between rounded-xl bg-white/5 px-3 py-2 ring-1 ring-white/10">
                <span>ASCII + batch</span>
                <span className="text-accent">Shared shading</span>
              </div>
              <div className="flex items-center justify-between rounded-xl bg-white/5 px-3 py-2 ring-1 ring-white/10">
                <span>GPU vs CPU marketing</span>
                <span className="text-amber-200">Held back</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
