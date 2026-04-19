"use client";

import { FormEvent, useState } from "react";
import { site } from "@/data/site";
import { SectionHeader } from "@/components/ui/SectionHeader";
import { Button } from "@/components/ui/Button";

export function ContactSection() {
  const [status, setStatus] = useState<"idle" | "sent">("idle");

  const onSubmit = (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const form = e.currentTarget;
    const data = new FormData(form);
    const name = String(data.get("name") || "").trim();
    const email = String(data.get("email") || "").trim();
    const message = String(data.get("message") || "").trim();
    const subject = encodeURIComponent(
      `[${site.projectName}] Note from ${name || "site visitor"}`,
    );
    const body = encodeURIComponent(`${message}\n\nFrom: ${email}`);
    window.location.href = `mailto:${site.email}?subject=${subject}&body=${body}`;
    setStatus("sent");
    form.reset();
  };

  return (
    <section
      id="contribute"
      className="scroll-mt-28 space-y-10 border-t border-white/5 bg-gradient-to-b from-surface-elevated to-surface py-16 sm:py-20"
    >
      <div className="mx-auto grid max-w-6xl gap-10 px-4 sm:px-6 lg:grid-cols-2 lg:px-8">
        <SectionHeader
          eyebrow="Contribute"
          title="Issues, docs, and source"
          description="Use GitHub for bugs and ideas; the README is the canonical quick start. Prefer opening an issue before one-off email so the whole community sees the thread."
        />
        <div className="space-y-6 rounded-2xl border border-white/10 bg-white/[0.03] p-6 shadow-[0_30px_120px_-80px_rgba(45,212,191,0.65)]">
          <div className="space-y-3 text-sm text-slate-300">
            <p>
              <span className="text-slate-500">Repository · </span>
              <a
                className="text-accent underline-offset-4 hover:underline"
                href={site.githubUrl}
                target="_blank"
                rel="noreferrer"
              >
                {site.repoLabel}
              </a>
            </p>
            <p>
              <span className="text-slate-500">README · </span>
              <a
                className="text-accent underline-offset-4 hover:underline"
                href={site.readmeUrl}
                target="_blank"
                rel="noreferrer"
              >
                Build and run
              </a>
            </p>
            <p>
              <span className="text-slate-500">Issues · </span>
              <a
                className="text-accent underline-offset-4 hover:underline"
                href={site.issuesUrl}
                target="_blank"
                rel="noreferrer"
              >
                Bug reports and feature ideas
              </a>
            </p>
            {site.email ? (
              <p>
                <span className="text-slate-500">Email · </span>
                <a
                  className="text-accent underline-offset-4 hover:underline"
                  href={`mailto:${site.email}`}
                >
                  {site.email}
                </a>{" "}
                <span className="text-xs text-slate-500">(optional direct line)</span>
              </p>
            ) : null}
          </div>

          <form className="space-y-4 border-t border-white/10 pt-6" onSubmit={onSubmit}>
            <p className="text-xs text-slate-500">
              Compose a private note via your mail client (set a real address in{" "}
              <code className="text-slate-400">src/data/site.ts</code> before publishing).
            </p>
            <div className="grid gap-4 sm:grid-cols-2">
              <label className="space-y-2 text-xs font-semibold uppercase tracking-wide text-slate-400">
                Name
                <input
                  name="name"
                  required
                  className="w-full rounded-xl border border-white/10 bg-surface px-3 py-2 text-sm text-white outline-none ring-accent/0 transition focus:border-accent/50 focus:ring-2 focus:ring-accent/30"
                  autoComplete="name"
                />
              </label>
              <label className="space-y-2 text-xs font-semibold uppercase tracking-wide text-slate-400">
                Email
                <input
                  name="email"
                  type="email"
                  required
                  className="w-full rounded-xl border border-white/10 bg-surface px-3 py-2 text-sm text-white outline-none ring-accent/0 transition focus:border-accent/50 focus:ring-2 focus:ring-accent/30"
                  autoComplete="email"
                />
              </label>
            </div>
            <label className="space-y-2 text-xs font-semibold uppercase tracking-wide text-slate-400">
              Message
              <textarea
                name="message"
                required
                rows={4}
                className="w-full rounded-xl border border-white/10 bg-surface px-3 py-2 text-sm text-white outline-none ring-accent/0 transition focus:border-accent/50 focus:ring-2 focus:ring-accent/30"
              />
            </label>
            <Button type="submit" className="w-full sm:w-auto">
              Open mail draft
            </Button>
            {status === "sent" ? (
              <p className="text-xs text-emerald-300" role="status">
                Draft launched — send from your mail client when ready.
              </p>
            ) : null}
          </form>
        </div>
      </div>
    </section>
  );
}
