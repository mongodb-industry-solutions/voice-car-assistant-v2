import { BACKEND_URL } from "@/lib/backend";

export const dynamic = "force-dynamic";

// Forward {text} to Piper and stream the WAV back.
export async function POST(request) {
  const body = await request.text();
  const upstream = await fetch(`${BACKEND_URL}/tts`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body,
    cache: "no-store",
  });
  if (!upstream.ok) {
    return new Response(await upstream.text(), { status: upstream.status });
  }
  return new Response(upstream.body, {
    status: 200,
    headers: { "Content-Type": "audio/wav" },
  });
}
