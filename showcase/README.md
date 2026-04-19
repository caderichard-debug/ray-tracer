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

## Deploy on Render.com

Blueprint: **`render.yaml`** at the **Git repository root** that contains `showcase/` (this tree uses **`ray-tracer/render.yaml`** when `ray-tracer` is the repo root). It defines a **Web Service** with `rootDir: showcase`, `npm ci && npm run build`, and `npm start`. If your remote root is one level up (parent folder also holds `poject-showcase/`, etc.), set **`rootDir: ray-tracer/showcase`** in `render.yaml` instead.

1. Push `render.yaml` to GitHub (same repo as this tracer).
2. In [Render Dashboard](https://dashboard.render.com) → **New** → **Blueprint** → connect the repo → apply the blueprint (or **New** → **Web Service** and set fields manually to match the YAML).
3. After the first successful deploy, open the service → **Environment** → add **`NEXT_PUBLIC_SITE_URL`** = your service URL (e.g. `https://ray-tracer-showcase.onrender.com`). Redeploy so Open Graph / `metadataBase` and JSON-LD use the right origin.
4. Do **not** set `STATIC_EXPORT` or `NEXT_PUBLIC_BASE_PATH` on Render unless you intentionally mirror the GitHub Pages layout; the app expects them unset for a normal `*.onrender.com` host.

The blueprint uses **`plan: free`** (idle spin-down, cold starts ~1 min). For always-on traffic, change to `starter` (or higher) in `render.yaml` or in the service **Settings**.

Manual Web Service (no blueprint): **Root Directory** = `showcase`, **Build** = `npm ci && npm run build`, **Start** = `npm start`, **Node** = 20.x.

## Deploy on Vercel

1. Create a new Vercel project and set the **root directory** to `showcase`.
2. Leave **Base Path** empty (default). Set `NEXT_PUBLIC_SITE_URL` to your Vercel URL (for example `https://ray-tracer-showcase.vercel.app`) in Project → Environment Variables.
3. Build command: `npm run build`, output: Next default (`.next`).

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
