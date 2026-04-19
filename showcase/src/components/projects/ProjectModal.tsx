"use client";

import Image from "next/image";
import { useEffect, useId, useRef } from "react";
import type { Project } from "@/types/project";
import { Button } from "@/components/ui/Button";

type ProjectModalProps = {
  project: Project | null;
  onClose: () => void;
};

export function ProjectModal({ project, onClose }: ProjectModalProps) {
  const closeRef = useRef<HTMLButtonElement>(null);
  const titleId = useId();

  useEffect(() => {
    if (!project) return;
    const prev = document.body.style.overflow;
    document.body.style.overflow = "hidden";
    closeRef.current?.focus();
    return () => {
      document.body.style.overflow = prev;
    };
  }, [project]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") onClose();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [onClose]);

  if (!project) return null;

  return (
    <div
      className="fixed inset-0 z-50 flex items-end justify-center bg-black/70 p-4 sm:items-center"
      role="presentation"
      onMouseDown={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div
        role="dialog"
        aria-modal="true"
        aria-labelledby={titleId}
        className="max-h-[90vh] w-full max-w-3xl overflow-y-auto rounded-2xl border border-white/10 bg-surface-elevated shadow-2xl shadow-black/60"
      >
        <div className="relative aspect-[16/9] w-full sm:aspect-[21/9]">
          <Image
            src={project.imageSrc}
            alt={project.imageAlt}
            fill
            className="object-cover"
            sizes="100vw"
            priority
          />
          <div className="absolute inset-0 bg-gradient-to-t from-surface-elevated via-surface-elevated/30 to-transparent" />
          <Button
            ref={closeRef}
            variant="ghost"
            className="absolute top-4 right-4 rounded-full bg-black/40 px-3 py-1 text-xs text-white ring-1 ring-white/20"
            onClick={onClose}
          >
            Close
          </Button>
        </div>

        <div className="space-y-6 px-6 py-6 sm:px-8 sm:py-8">
          <div className="flex flex-wrap items-center gap-3">
            <p className="text-xs font-semibold uppercase tracking-[0.2em] text-accent">
              {project.category}
            </p>
            {project.status === "wip" ? (
              <span className="rounded-full bg-amber-400/15 px-3 py-1 text-[11px] font-semibold text-amber-100 ring-1 ring-amber-300/40">
                Work in progress
              </span>
            ) : (
              <span className="rounded-full bg-emerald-400/10 px-3 py-1 text-[11px] font-semibold text-emerald-200 ring-1 ring-emerald-300/30">
                Production path
              </span>
            )}
          </div>

          <div className="space-y-2">
            <h2
              id={titleId}
              className="text-2xl font-semibold tracking-tight text-white sm:text-3xl"
            >
              {project.title}
            </h2>
            <p className="text-base text-slate-300">{project.tagline}</p>
          </div>

          <div className="grid gap-6 sm:grid-cols-2">
            <div className="space-y-2">
              <h3 className="text-sm font-semibold text-white">Problem</h3>
              <p className="text-sm leading-relaxed text-slate-400">
                {project.problem}
              </p>
            </div>
            <div className="space-y-2">
              <h3 className="text-sm font-semibold text-white">Solution</h3>
              <p className="text-sm leading-relaxed text-slate-400">
                {project.solution}
              </p>
            </div>
          </div>

          <div className="space-y-3">
            <h3 className="text-sm font-semibold text-white">Stack</h3>
            <div className="flex flex-wrap gap-2">
              {project.stack.map((tech) => (
                <span
                  key={tech}
                  className="rounded-full border border-white/10 bg-white/5 px-3 py-1 text-xs text-slate-200"
                >
                  {tech}
                </span>
              ))}
            </div>
          </div>

          <div className="space-y-3">
            <h3 className="text-sm font-semibold text-white">Results</h3>
            <ul className="list-disc space-y-2 pl-5 text-sm text-slate-300">
              {project.results.map((line) => (
                <li key={line} className="leading-relaxed">
                  {line}
                </li>
              ))}
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
}
