import type { Metadata } from "next";
import { Geist, Geist_Mono } from "next/font/google";
import "./globals.css";
import { site } from "@/data/site";

const geistSans = Geist({
  variable: "--font-geist-sans",
  subsets: ["latin"],
});

const geistMono = Geist_Mono({
  variable: "--font-geist-mono",
  subsets: ["latin"],
});

const siteUrl =
  process.env.NEXT_PUBLIC_SITE_URL || "https://caderichard-debug.github.io/ray-tracer";

export const metadata: Metadata = {
  metadataBase: new URL(siteUrl),
  title: {
    default: `${site.projectName} — project showcase`,
    template: `%s · ${site.projectName}`,
  },
  description: site.description,
  openGraph: {
    title: `${site.projectName} — project showcase`,
    description: site.description,
    url: "/",
    siteName: site.projectName,
    type: "website",
    locale: "en_US",
    images: [{ url: site.ogImage, width: 1200, height: 630, alt: site.tagline }],
  },
  twitter: {
    card: "summary_large_image",
    title: `${site.projectName} — project showcase`,
    description: site.description,
    images: [site.ogImage],
  },
  alternates: {
    canonical: "/",
  },
  robots: {
    index: true,
    follow: true,
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  const jsonLd = {
    "@context": "https://schema.org",
    "@type": "SoftwareSourceCode",
    name: site.projectName,
    description: site.description,
    codeRepository: site.githubUrl,
    license: site.licenseUrl,
    programmingLanguage: ["C++", "GLSL"],
    url: siteUrl,
  };

  const basePath = process.env.NEXT_PUBLIC_BASE_PATH ?? "";

  return (
    <html lang="en">
      <body
        className={`${geistSans.variable} ${geistMono.variable} min-h-screen bg-[#f1ece0] antialiased`}
      >
        <a
          href={`${basePath}/Ray%20Tracer.html#abstract`}
          className="sr-only focus:not-sr-only focus:absolute focus:left-4 focus:top-4 focus:z-[100] focus:rounded-full focus:bg-[#1a1714] focus:px-4 focus:py-2 focus:text-[#f1ece0]"
        >
          Skip to abstract (editorial page)
        </a>
        <script
          type="application/ld+json"
          dangerouslySetInnerHTML={{ __html: JSON.stringify(jsonLd) }}
        />
        {children}
      </body>
    </html>
  );
}
