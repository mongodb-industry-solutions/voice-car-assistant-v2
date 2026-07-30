import { proxy } from "@/lib/backend";

export const dynamic = "force-dynamic";

// Forward the uploaded audio blob (multipart) to the backend Whisper endpoint.
export async function POST(request) {
  const contentType = request.headers.get("content-type") || "application/octet-stream";
  const buf = await request.arrayBuffer();
  return proxy("/stt", {
    method: "POST",
    headers: { "Content-Type": contentType },
    body: buf,
  });
}
