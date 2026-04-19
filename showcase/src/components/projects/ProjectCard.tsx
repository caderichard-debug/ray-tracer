import Image from "next/image";
import type { Project } from "@/types/project";

type ProjectCardProps = {
  project: Project;
  onOpen: (project: Project) => void;
};

export function ProjectCard({ project, onOpen }: ProjectCardProps) {
  return (
    <article className="group flex h-full flex-col overflow-hidden rounded-2xl border border-white/10 bg-white/[0.03] shadow-[0_20px_80px_-40px_rgba(0,0,0,0.8)] transition duration-300 hover:-translate-y-1 hover:border-accent/30 hover:shadow-[0_30px_120px_-50px_rgba(45,212,191,0.35)]">
      <button
        type="button"
        onClick={() => onOpen(project)}
        className="flex h-full flex-col text-left"
        aria-haspopup="dialog"
      >
        <div className="relative aspect-[16/10] overflow-hidden">
          <Image
            src={project.imageSrc}
            alt={project.imageAlt}
            fill
            sizes="(min-width: 1024px) 33vw, (min-width: 768px) 50vw, 100vw"
            className="object-cover transition duration-500 group-hover:scale-[1.03]"
            priority={project.featured}
          />
          <div className="absolute inset-0 bg-gradient-to-t from-surface via-surface/40 to-transparent" />
          <div className="absolute left-4 right-4 bottom-4 flex items-center justify-between gap-2">
            <div>
              <p className="text-xs uppercase tracking-wide text-accent/90">
                {project.category}
              </p>
              <h3 className="text-lg font-semibold text-white">{project.title}</h3>
            </div>
            {project.status === "wip" ? (
              <span className="rounded-full bg-amber-400/15 px-3 py-1 text-[11px] font-semibold text-amber-200 ring-1 ring-amber-300/30">
                WIP
              </span>
            ) : null}
          </div>
        </div>
        <div className="flex flex-1 flex-col gap-3 px-5 py-4">
          <p className="text-sm text-slate-400">{project.tagline}</p>
          <div className="mt-auto flex flex-wrap gap-2">
            {project.stack.slice(0, 4).map((tech) => (
              <span
                key={tech}
                className="rounded-full bg-white/5 px-2 py-1 text-[11px] text-slate-300 ring-1 ring-white/10"
              >
                {tech}
              </span>
            ))}
          </div>
        </div>
      </button>
    </article>
  );
}
