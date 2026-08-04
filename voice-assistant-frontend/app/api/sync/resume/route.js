import { proxy } from "@/lib/backend";
export const dynamic = "force-dynamic";
export async function POST() {
  return proxy("/api/sync/resume", { method: "POST" });
}
