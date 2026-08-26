import { proxy } from "@/lib/backend";
export const dynamic = "force-dynamic";
export async function GET(request) {
  // Forward the query string (vehicleId in session scope; ignored in global scope).
  const qs = new URL(request.url).search;
  return proxy(`/api/sync/state${qs}`);
}
