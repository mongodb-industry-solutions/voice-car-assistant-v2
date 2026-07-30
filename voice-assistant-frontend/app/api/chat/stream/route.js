import { BACKEND_URL } from "@/lib/backend";

export const dynamic = "force-dynamic";

// Proxy the backend SSE stream straight through to the browser.
export async function POST(request) {
  const body = await request.text();
  const upstream = await fetch(`${BACKEND_URL}/chat/stream`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body,
    cache: "no-store",
  });

  return new Response(upstream.body, {
    status: upstream.status,
    headers: {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      "X-Accel-Buffering": "no",
    },
  });
}
