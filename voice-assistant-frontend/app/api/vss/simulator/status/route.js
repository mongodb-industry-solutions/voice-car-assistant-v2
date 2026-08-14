import { proxy } from "@/lib/backend";
export const dynamic = "force-dynamic";
export async function GET(request) {
  // Forward the query string so the per-session vehicleId reaches the backend.
  const qs = new URL(request.url).search;
  return proxy(`/api/vss/simulator/status${qs}`);
}
