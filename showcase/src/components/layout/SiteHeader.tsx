import Link from "next/link";
import { site } from "@/data/site";

const nav = [
  { href: "#showcase", label: "Showcase" },
  { href: "#about", label: "About" },
  { href: "#skills", label: "Stack" },
  { href: "#highlights", label: "Highlights" },
  { href: "#contribute", label: "Contribute" },
];

export function SiteHeader() {
  return (
    <header className="sticky top-0 z-40 border-b border-white/5 bg-surface/80 backdrop-blur-xl">
      <div className="mx-auto flex max-w-6xl items-center justify-between gap-4 px-4 py-4 sm:px-6 lg:px-8">
        <Link
          href="/"
          className="text-sm font-semibold tracking-tight text-white transition hover:text-accent"
        >
          {site.projectName}
        </Link>
        <nav
          className="flex flex-wrap items-center justify-end gap-x-4 gap-y-2 text-sm text-slate-300"
          aria-label="Primary"
        >
          {nav.map((item) => (
            <a
              key={item.href}
              href={item.href}
              className="transition hover:text-white"
            >
              {item.label}
            </a>
          ))}
          <a
            href={site.githubUrl}
            className="rounded-full border border-white/10 px-3 py-1 text-xs font-semibold text-white transition hover:border-accent/50 hover:text-accent"
            target="_blank"
            rel="noreferrer"
          >
            GitHub
          </a>
        </nav>
      </div>
    </header>
  );
}
