/** @type {import('next').NextConfig} */
const basePath = process.env.NEXT_PUBLIC_BASE_PATH || "";
const staticExport = process.env.STATIC_EXPORT === "1";

const nextConfig = {
  ...(staticExport ? { output: "export" } : {}),
  ...(basePath ? { basePath, assetPrefix: `${basePath}/` } : {}),
  trailingSlash: true,
  images: {
    unoptimized: staticExport,
    remotePatterns: [
      {
        protocol: "https",
        hostname: "raw.githubusercontent.com",
        pathname: "/caderichard-debug/ray-tracer/**",
      },
    ],
  },
};

export default nextConfig;
