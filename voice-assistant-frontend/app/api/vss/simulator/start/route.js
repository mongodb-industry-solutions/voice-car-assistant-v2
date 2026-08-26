import { proxy } from "@/lib/backend";
export const dynamic = "force-dynamic";
export async function POST(request) {
  // Forward the request body (carries vehicle_id for the per-session vehicle).
  const body = await request.text();
  return proxy("/api/vss/simulator/start", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: body || "{}",
  });
}
