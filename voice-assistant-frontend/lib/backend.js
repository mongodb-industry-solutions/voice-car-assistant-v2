// Server-side backend base URL. Never exposed to the browser (not NEXT_PUBLIC_*).
export const BACKEND_URL = process.env.BACKEND_URL || "http://localhost:8080";

// Small JSON proxy helper for the simple GET/POST routes.
export async function proxy(path, init) {
  const resp = await fetch(`${BACKEND_URL}${path}`, { ...init, cache: "no-store" });
  const body = await resp.text();
  return new Response(body, {
    status: resp.status,
    headers: { "Content-Type": resp.headers.get("Content-Type") || "application/json" },
  });
}
