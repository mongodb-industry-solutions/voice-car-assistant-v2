import { proxy } from "@/lib/backend";
export const dynamic = "force-dynamic";
export async function POST() {
  return proxy("/api/vss/simulator/start", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: "{}",
  });
}
