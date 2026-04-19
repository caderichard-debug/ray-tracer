import { site } from "@/data/site";

export function SiteFooter() {
  return (
    <footer className="border-t border-white/5 bg-surface py-10 text-sm text-slate-500">
      <div className="mx-auto flex max-w-6xl flex-col gap-4 px-4 sm:flex-row sm:items-center sm:justify-between sm:px-6 lg:px-8">
        <p>
          {site.projectName} · {site.licenseName}{" "}
          <a
            className="text-slate-300 underline-offset-4 hover:text-accent hover:underline"
            href={site.licenseUrl}
            target="_blank"
            rel="noreferrer"
          >
            License
          </a>{" "}
          ·{" "}
          <a
            className="text-slate-300 underline-offset-4 hover:text-accent hover:underline"
            href={site.githubUrl}
            target="_blank"
            rel="noreferrer"
          >
            {site.repoLabel}
          </a>
        </p>
        <p className="text-xs text-slate-600">
          Marketing site built with Next.js in <code className="text-slate-500">showcase/</code>.
          Maintained by {site.maintainer}
          {site.maintainerLinkedIn ? (
            <>
              {" · "}
              <a
                className="text-slate-400 underline-offset-4 hover:text-accent hover:underline"
                href={site.maintainerLinkedIn}
                target="_blank"
                rel="noreferrer"
              >
                LinkedIn
              </a>
            </>
          ) : null}
          . CPU renderer is the product focus; GPU mode is WIP until parity and CI
          benchmarks stabilize.
        </p>
      </div>
    </footer>
  );
}
