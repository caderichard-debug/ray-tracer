# Showcase site (Next.js)

Project-focused marketing site for **SIMD Ray Tracer**: hero, filterable surface grid with modal detail, about, implementation stack, repo proof points, and contribute / mail links.

## Local development

```bash
cd showcase
npm install
npm run dev
```

Open `http://localhost:3000` (or `npm run dev -- -p 3001` for port **3001**).

**`/` (home)** loads the editorial handoff in a full-viewport **iframe** pointed at `public/Ray Tracer.html`, so what you see on the dev server matches the Claude Design file (Fraunces / paper / benchmarks layout), not the older dark Next.js marketing page.

## Claude Design handoff (`Ray Tracer.html`)

**Source of truth:** the unzipped tree **`poject-showcase/project/Ray Tracer.html`** (repo root next to `ray-tracer/`). After editing the design, copy it into **`public/Ray Tracer.html`** and re-apply any local tweaks (e.g. hero/gallery `<img>` slots that pull from `readme-examples/` on GitHub).

The file is self-contained HTML/CSS/JS (Fraunces, Inter Tight, JetBrains Mono; light/dark; tweak panel). **`public/Ray Tracer.html`** is the served copy.

With `next dev`: **`http://localhost:3000/Ray%20Tracer.html`**. With static export and `basePath=/ray-tracer`: **`/ray-tracer/Ray%20Tracer.html`**.

The Next app at `/` is separate; this file is the editorial handoff page from design.

## Static-only deployment model

This showcase is configured as a **static-export-only** Next.js app:

- `showcase/next.config.mjs` sets `output: "export"` unconditionally.
- Build output is `showcase/out/`.
- There is **no SSR runtime** and no backend/server requirement.
- `npm start` is intentionally disabled for this package.

Local CI-style check: `npm run build:prod-install`. Full clean tree: `npm run build:clean`.

## Deploy on GitHub Pages (project site)

1. In the GitHub repo, enable **Pages** with source **GitHub Actions** (not `/docs` on `main` unless you mirror output there yourself).
2. Merge the workflow at `.github/workflows/showcase-pages.yml`. Pushes under `showcase/` trigger a static export with `basePath=/ray-tracer` and upload the `out/` directory.
3. First successful deploy will be served at `https://caderichard-debug.github.io/ray-tracer/`.

To build the static bundle locally:

```bash
cd showcase
npm run build:static
# output in showcase/out
```

## Customize

- Copy and edit `src/data/site.ts` (name, links, email).
- Project cards and modal copy live in `src/data/projects.ts`.
- Remote gallery images default to `raw.githubusercontent.com/.../readme-examples/`; swap URLs or add files under `public/` if you prefer self-hosted assets.

The legacy multi-page HTML under `docs/` is optional; this app supersedes it once Pages is wired to the workflow above.
