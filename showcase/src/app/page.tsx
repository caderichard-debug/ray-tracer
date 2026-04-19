"use client";

/**
 * Root URL shows the Claude Design handoff (`public/Ray Tracer.html`) so the
 * dev server matches the editorial prototype. Sync source from
 * `poject-showcase/project/Ray Tracer.html` → `public/Ray Tracer.html`.
 */
export default function Home() {
  const base = process.env.NEXT_PUBLIC_BASE_PATH ?? "";
  const src = `${base}/Ray%20Tracer.html`;

  return (
    <iframe
      title="Ray Tracer — editorial handoff (Claude Design)"
      src={src}
      className="fixed inset-0 z-0 block h-[100dvh] w-full border-0"
    />
  );
}
