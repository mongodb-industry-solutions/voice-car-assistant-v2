import { proxy } from "@/lib/backend";
export const dynamic = "force-dynamic";
export async function GET(request) {
  // Forward the query string (vehicleId for the per-session vehicle, plus cache-buster).
  const qs = new URL(request.url).search;
  return proxy(`/api/vss/latest${qs}`);
}
