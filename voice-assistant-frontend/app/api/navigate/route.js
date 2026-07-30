import { proxy } from "@/lib/backend";
export const dynamic = "force-dynamic";
export async function POST(request) {
  const body = await request.text();
  return proxy("/api/navigate", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body,
  });
}
