/** @type {import('next').NextConfig} */
const nextConfig = {
  // Standalone output → small runtime image for Kanopy.
  output: "standalone",
  // Backend base URL is read server-side only (never NEXT_PUBLIC_*), per the
  // Kanopy proxy pattern; the browser only ever talks to same-origin /api/*.
};

export default nextConfig;
