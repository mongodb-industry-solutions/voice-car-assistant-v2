import { proxy } from "@/lib/backend";
export const dynamic = "force-dynamic";
export async function GET() {
  return proxy("/api/sync/state");
}
