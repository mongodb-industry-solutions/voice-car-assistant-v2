import { proxy } from "@/lib/backend";
export const dynamic = "force-dynamic";
export async function POST(request) {
  // Forward the body (carries vehicle_id in session scope; ignored in global scope).
  const body = await request.text();
  return proxy("/api/sync/resume", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: body || "{}",
  });
}
