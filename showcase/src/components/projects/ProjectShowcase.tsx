"use client";

import { useMemo, useState } from "react";
import { projects, categories, allStacks } from "@/data/projects";
import type { Project, ProjectCategory } from "@/types/project";
import { SectionHeader } from "@/components/ui/SectionHeader";
import { Pill } from "@/components/ui/Pill";
import { ProjectCard } from "@/components/projects/ProjectCard";
import { ProjectModal } from "@/components/projects/ProjectModal";

export function ProjectShowcase() {
  const [category, setCategory] = useState<(typeof categories)[number]>("All");
  const [stack, setStack] = useState<string>("All");
  const [active, setActive] = useState<Project | null>(null);

  const filtered = useMemo(() => {
    return projects.filter((project) => {
      const categoryOk =
        category === "All" || project.category === (category as ProjectCategory);
      const stackOk =
        stack === "All" || project.stack.map((s) => s.toLowerCase()).includes(stack.toLowerCase());
      return categoryOk && stackOk;
    });
  }, [category, stack]);

  return (
    <section
      id="showcase"
      className="scroll-mt-28 border-t border-white/5 bg-gradient-to-b from-surface to-surface-elevated py-16 sm:py-20"
    >
      <div className="mx-auto max-w-6xl space-y-10 px-4 sm:px-6 lg:px-8">
      <SectionHeader
        eyebrow="In this repository"
        title="Surfaces and tooling"
        description="Every card is part of this ray tracer: how it runs interactively, how batch output stays deterministic, how ASCII reuses the same core, and where the GPU path sits while it is still WIP."
      />

      <div className="flex flex-col gap-4 lg:flex-row lg:items-end lg:justify-between">
        <div className="space-y-2">
          <p className="text-xs font-semibold uppercase tracking-[0.2em] text-slate-500">
            Area
          </p>
          <div className="flex flex-wrap gap-2">
            {categories.map((c) => (
              <Pill
                key={c}
                active={category === c}
                onClick={() => setCategory(c)}
              >
                {c}
              </Pill>
            ))}
          </div>
        </div>
        <div className="space-y-2">
          <p className="text-xs font-semibold uppercase tracking-[0.2em] text-slate-500">
            Tech
          </p>
          <div className="flex flex-wrap gap-2">
            <Pill active={stack === "All"} onClick={() => setStack("All")}>
              All stacks
            </Pill>
            {allStacks.map((tech) => (
              <Pill
                key={tech}
                active={stack === tech}
                onClick={() => setStack(tech)}
              >
                {tech}
              </Pill>
            ))}
          </div>
        </div>
      </div>

      <div className="grid gap-6 md:grid-cols-2 xl:grid-cols-3">
        {filtered.map((project) => (
          <ProjectCard
            key={project.id}
            project={project}
            onOpen={(p) => setActive(p)}
          />
        ))}
      </div>

      {filtered.length === 0 ? (
        <p className="text-sm text-slate-400">
          Nothing matches that combination — reset filters to see the full grid.
        </p>
      ) : null}

      <ProjectModal project={active} onClose={() => setActive(null)} />
      </div>
    </section>
  );
}
