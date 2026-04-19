# GitHub Pages — handoff instructions for the next agent

This document tells you how to publish a **GitHub Pages** marketing/docs site for this repository. The **CPU renderer is the product focus**; **GPU is WIP**—keep that framing on the site.

## Repository facts (do not re-derive from scratch)

- **Remote:** `https://github.com/caderichard-debug/ray-tracer`
- **Default branch:** `main`
- **Primary entrypoints:**
  - `README.md` — CPU-first narrative, quick start, controls
  - `LLM_CONTEXT.md` — manager/agent coordination context (CPU-first, GPU WIP)
  - `claude-raytracer-cpu.md` — CPU agent deep context
  - `docs/path-tracing-plan.md` — roadmap / design notes
- **Visual assets already in-repo:** `readme-examples/*.png` (Cornell box renders)
- **Build system:** root `Makefile` (`make interactive-cpu`, `make batch-cpu`, `make regression-test`, …)

## Goal of the Pages site

Ship a **small, fast, static** site that:

- Explains what the project is (**CPU ray tracer**, SIMD + OpenMP, SDL2 interactive + batch)
- Shows **screenshots** (reuse `readme-examples/` images; optionally add new curated PNGs under `docs/assets/` if needed)
- Documents **how to build/run** (copy the minimal commands from `README.md`)
- Clearly labels **GPU renderer as WIP** (no “60–300×” style claims unless re-measured and reproducible)
- Links to **GitHub** source, issues, and (if present) discussions

Non-goals for v1:

- Hosting binaries
- Running the tracer in the browser (unless you explicitly choose a separate WASM project)

## Choose a publishing mechanism (pick one)

### Option A — `docs/` folder on `main` (simplest operationally)

1. Enable GitHub Pages in repo settings: **Build from branch `main` / folder `/docs`**.
2. Put the generated site under `docs/` (e.g., `docs/index.html` + `docs/assets/...`).
3. Commit to `main` and verify Pages build logs.

**Pros:** no extra branch. **Cons:** mixes site sources with repo docs unless you namespace carefully (e.g., `docs/site/`).

### Option B — GitHub Actions → `gh-pages` branch (common for static generators)

1. Add `.github/workflows/pages.yml` that builds a static site and deploys with `actions/upload-pages-artifact` + `actions/deploy-pages`.
2. Configure repo Pages settings: **GitHub Actions** as source.
3. Keep generated output **out of `main`** (recommended).

**Pros:** clean separation. **Cons:** more YAML + permissions setup.

### Option C — User/organization site repo

Only if the user explicitly wants `username.github.io` instead of a **project site**.

## Recommended site structure (content outline)

Create these pages (names are suggestions):

1. **`index` / Home**
   - One-sentence pitch: CPU-first SIMD ray tracer + SDL2 interactive
   - 3-image gallery (reuse README’s Cornell table crops if helpful)
   - “Get started” button → Build & Run

2. **`features`**
   - CPU interactive: progressive / adaptive / wavefront toggles, analysis modes, denoise pipeline (high level)
   - Mention **window screenshots** and **translucent panel** as UX polish (no need to paste code)

3. **`build`**
   - Copy/paste **minimal** commands from `README.md` (Homebrew/apt/MSYS2 + `make interactive-cpu`)
   - Mention `make regression-test` exists for deterministic CPU batch checks

4. **`roadmap`**
   - GPU WIP, what “done” might mean (stability, parity, benchmarks)

5. **`community`**
   - Link to GitHub: code, issues, contributing expectations

## Implementation notes (engineering)

### If you use Jekyll (GitHub Pages default)

- Add `docs/Gemfile` only if you need plugins; keep it minimal.
- Set `baseurl` correctly for a **project site**:
  - Project pages URL shape: `https://<user>.github.io/ray-tracer/`
  - `baseurl: "/ray-tracer"` (verify against GitHub Pages URL)
- Use `relative_url` filters for navigation so assets resolve on project pages.

### If you use plain static HTML

- Prefer **relative** asset paths (`assets/...`) to avoid host assumptions.
- Keep CSS minimal; avoid heavy JS frameworks unless necessary.

### Assets

- Prefer existing `readme-examples/` images to avoid bloating git with duplicates.
- If you add new images, keep them **web-optimized** (reasonable dimensions; compress PNGs).

### Accessibility / quality bar

- Every image needs meaningful `alt` text.
- Ensure contrast for body text.
- Don’t autoplay video (none expected).

## Verification checklist before declaring victory

- [ ] Pages build succeeds (Actions logs or Jekyll build output)
- [ ] Home page loads on the final GitHub Pages URL (not just locally)
- [ ] All internal links work with the chosen `baseurl` strategy
- [ ] Screenshots render (no broken images)
- [ ] Copy matches repo reality: **CPU-first**, **GPU WIP**
- [ ] No secrets committed (tokens, `.env`, keys)

## Suggested first PR scope

Keep PR #1 small:

- Add `docs/` site skeleton + styling
- Wire GitHub Pages (Option A or B)
- Link prominently to the GitHub repo

Defer to PR #2:

- Blog posts, deep-dive shader notes, benchmark dashboards

## Questions to ask the user if anything is ambiguous

- Project site vs user site (`username.github.io`)?
- Custom domain + DNS (CNAME) desired?
- Should the site include **benchmark_results/** charts (often noisy / large), or stay minimal?
