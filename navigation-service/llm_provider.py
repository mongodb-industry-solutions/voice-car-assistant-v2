"""
Text-completion provider selector — local Ollama vs cloud Grove (hosted Claude).

`LLM_PROVIDER` (default "ollama") picks the implementation. Both expose one call,
`complete(prompt) -> str`, so navigation's intent parser is provider-agnostic:
local runs Ollama, Kanopy runs Grove-hosted Claude with no code change.
"""

import os

LLM_PROVIDER = os.getenv("LLM_PROVIDER", "ollama").strip().lower()
OLLAMA_HOST  = os.getenv("OLLAMA_HOST", "http://localhost:11434")
LLM_MODEL    = os.getenv("LLM_MODEL", "qwen3:4b")

# Grove config comes entirely from env/secrets — no endpoint URL hardcoded in the repo.
_MODEL_ID       = os.getenv("GROVE_MODEL_ID")
_GROVE_BASE_URL = os.getenv("GROVE_BASE_URL")

_grove_client = None


def _grove():
    global _grove_client
    if _grove_client is None:
        import anthropic
        key = os.getenv("GROVE_API_KEY")
        missing = [n for n, v in (("GROVE_API_KEY", key), ("GROVE_MODEL_ID", _MODEL_ID),
                                  ("GROVE_BASE_URL", _GROVE_BASE_URL)) if not v]
        if missing:
            raise ValueError(f"LLM_PROVIDER=grove requires {', '.join(missing)}")
        # Grove authenticates with `api-key`, not Anthropic's own `x-api-key`.
        _grove_client = anthropic.Anthropic(base_url=_GROVE_BASE_URL, api_key=key,
                                             default_headers={"api-key": key})
    return _grove_client


def complete(prompt: str) -> str:
    """Return the model's text completion for a single-turn prompt."""
    if LLM_PROVIDER == "grove":
        msg = _grove().messages.create(
            model=_MODEL_ID,
            max_tokens=512,
            messages=[{"role": "user", "content": prompt}],
        )
        return "".join(b.text for b in msg.content if getattr(b, "type", None) == "text").strip()

    if LLM_PROVIDER == "ollama":
        import ollama
        resp = ollama.Client(host=OLLAMA_HOST).generate(model=LLM_MODEL, prompt=prompt, think=False)
        return (resp.get("response") or "").strip()

    raise ValueError(f"unsupported LLM_PROVIDER {LLM_PROVIDER!r} (expected 'ollama' or 'grove')")
