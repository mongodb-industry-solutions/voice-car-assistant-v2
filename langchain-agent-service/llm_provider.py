"""
Chat-model provider selector — local Ollama vs cloud Grove (hosted Claude).

Mirrors the coexistence pattern from the leafy-wallet / retail-stock-take demos:
one env var (`LLM_PROVIDER`, default "ollama") picks the implementation at import,
so a bare checkout runs fully local and Kanopy runs the cloud model with no code
change. Both branches return a LangChain BaseChatModel, so the agent graph is
provider-agnostic.

Grove is MongoDB's gateway to hosted Claude, exposed as an Anthropic-compatible
API. It authenticates with an `api-key` header (not Anthropic's `x-api-key`) and
is reached at a Grove gateway base URL — same as leafy-wallet's graph.js.
"""

import os

LLM_PROVIDER = os.getenv("LLM_PROVIDER", "ollama").strip().lower()

# Grove config comes entirely from env/secrets — no endpoint URL hardcoded in the repo.
_MODEL_ID       = os.getenv("GROVE_MODEL_ID")
_GROVE_BASE_URL = os.getenv("GROVE_BASE_URL")


def make_chat_llm():
    """Return the configured LangChain chat model for the selected provider."""
    if LLM_PROVIDER == "grove":
        from langchain_anthropic import ChatAnthropic

        api_key = os.getenv("GROVE_API_KEY")
        missing = [n for n, v in (("GROVE_API_KEY", api_key), ("GROVE_MODEL_ID", _MODEL_ID),
                                  ("GROVE_BASE_URL", _GROVE_BASE_URL)) if not v]
        if missing:
            raise ValueError(f"LLM_PROVIDER=grove requires {', '.join(missing)}")
        print(f"LLM provider: grove (model={_MODEL_ID})", flush=True)
        return ChatAnthropic(
            model=_MODEL_ID,
            temperature=0,
            max_tokens=1024,
            anthropic_api_url=_GROVE_BASE_URL,
            api_key=api_key,
            # Grove authenticates with `api-key`, not Anthropic's own `x-api-key`.
            default_headers={"api-key": api_key},
        )

    if LLM_PROVIDER == "ollama":
        from langchain_ollama import ChatOllama

        print(f"LLM provider: ollama (model={os.getenv('LLM_MODEL', 'qwen2.5:3b')})", flush=True)
        return ChatOllama(
            model=os.getenv("LLM_MODEL", "qwen2.5:3b"),
            base_url=os.getenv("OLLAMA_HOST", "http://localhost:11434"),
            temperature=0,
            keep_alive=-1,
            num_ctx=1536,
            num_predict=400,
        )

    raise ValueError(f"unsupported LLM_PROVIDER {LLM_PROVIDER!r} (expected 'ollama' or 'grove')")
