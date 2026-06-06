#!/usr/bin/env python3
"""Small LAN gateway for the VoiceMemoReminder Arduino example.

Default mode is dependency-free and returns a mock transcript/memo, which lets
you verify the reTerminal recording, upload, and screen UI first.

Useful environment variables:
  STT_BACKEND=mock | faster_whisper | openai_compatible
  CHAT_BACKEND=rule | openai_compatible
  STT_MODEL=small
  STT_BASE_URL=https://api.openai.com/v1
  STT_API_KEY=...
  CHAT_BASE_URL=http://localhost:11434/v1
  CHAT_API_KEY=ollama
  CHAT_MODEL=qwen2.5:3b
"""

from __future__ import annotations

import argparse
import json
import os
import tempfile
import urllib.error
import urllib.request
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Dict, Tuple


_WHISPER_MODEL = None


def env(name: str, default: str = "") -> str:
    return os.environ.get(name, default).strip()


def json_response(handler: BaseHTTPRequestHandler, status: int, payload: Dict[str, str]) -> None:
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(data)))
    handler.end_headers()
    handler.wfile.write(data)


def multipart_form(fields: Dict[str, str], file_field: str, filename: str,
                   content_type: str, file_bytes: bytes) -> Tuple[bytes, str]:
    boundary = "----voice-memo-" + uuid.uuid4().hex
    chunks = []
    for name, value in fields.items():
        chunks.append(f"--{boundary}\r\n".encode())
        chunks.append(f'Content-Disposition: form-data; name="{name}"\r\n\r\n'.encode())
        chunks.append(value.encode("utf-8"))
        chunks.append(b"\r\n")
    chunks.append(f"--{boundary}\r\n".encode())
    chunks.append(
        f'Content-Disposition: form-data; name="{file_field}"; filename="{filename}"\r\n'
        f"Content-Type: {content_type}\r\n\r\n".encode()
    )
    chunks.append(file_bytes)
    chunks.append(b"\r\n")
    chunks.append(f"--{boundary}--\r\n".encode())
    return b"".join(chunks), boundary


def http_json(url: str, payload: Dict, api_key: str = "") -> Dict:
    data = json.dumps(payload).encode("utf-8")
    headers = {"Content-Type": "application/json"}
    if api_key:
        headers["Authorization"] = "Bearer " + api_key
    request = urllib.request.Request(url, data=data, headers=headers, method="POST")
    with urllib.request.urlopen(request, timeout=120) as response:
        return json.loads(response.read().decode("utf-8"))


def http_multipart(url: str, fields: Dict[str, str], api_key: str, audio: bytes) -> Dict:
    body, boundary = multipart_form(fields, "file", "memo.wav", "audio/wav", audio)
    headers = {"Content-Type": "multipart/form-data; boundary=" + boundary}
    if api_key:
        headers["Authorization"] = "Bearer " + api_key
    request = urllib.request.Request(url, data=body, headers=headers, method="POST")
    with urllib.request.urlopen(request, timeout=180) as response:
        return json.loads(response.read().decode("utf-8"))


def transcribe_mock(_: bytes) -> str:
    return env("MOCK_TRANSCRIPT", "Remind me to check the sensor logs tomorrow morning.")


def transcribe_faster_whisper(audio: bytes) -> str:
    global _WHISPER_MODEL
    try:
        from faster_whisper import WhisperModel
    except ImportError as exc:
        raise RuntimeError("Install faster-whisper or use STT_BACKEND=mock") from exc

    if _WHISPER_MODEL is None:
        model_name = env("STT_MODEL", "small")
        device = env("WHISPER_DEVICE", "cpu")
        compute_type = env("WHISPER_COMPUTE_TYPE", "int8")
        _WHISPER_MODEL = WhisperModel(model_name, device=device, compute_type=compute_type)

    with tempfile.NamedTemporaryFile(suffix=".wav") as wav:
        wav.write(audio)
        wav.flush()
        segments, _ = _WHISPER_MODEL.transcribe(
            wav.name,
            language=env("STT_LANGUAGE", None) or None,
            vad_filter=True,
        )
        return " ".join(segment.text.strip() for segment in segments).strip()


def transcribe_openai_compatible(audio: bytes) -> str:
    base_url = env("STT_BASE_URL", "https://api.openai.com/v1").rstrip("/")
    model = env("STT_MODEL", "whisper-1")
    payload = http_multipart(
        base_url + "/audio/transcriptions",
        {"model": model, "response_format": "json"},
        env("STT_API_KEY"),
        audio,
    )
    return str(payload.get("text", "")).strip()


def transcribe(audio: bytes) -> str:
    backend = env("STT_BACKEND", "mock")
    if backend == "mock":
        return transcribe_mock(audio)
    if backend == "faster_whisper":
        return transcribe_faster_whisper(audio)
    if backend == "openai_compatible":
        return transcribe_openai_compatible(audio)
    raise RuntimeError(f"Unknown STT_BACKEND={backend}")


def summarize_rule(transcript: str) -> str:
    text = " ".join(transcript.split())
    if not text:
        return "No speech recognized."
    prefixes = [
        "remind me to ",
        "please remind me to ",
        "remember to ",
        "note that ",
    ]
    lowered = text.lower()
    for prefix in prefixes:
        if lowered.startswith(prefix):
            text = text[len(prefix):]
            break
    text = text[:1].upper() + text[1:]
    if text[-1] not in ".!?":
        text += "."
    return text


def summarize_openai_compatible(transcript: str) -> str:
    base_url = env("CHAT_BASE_URL", "http://localhost:11434/v1").rstrip("/")
    model = env("CHAT_MODEL", "qwen2.5:3b")
    payload = {
        "model": model,
        "temperature": 0.2,
        "messages": [
            {
                "role": "system",
                "content": (
                    "Turn the user's speech transcript into exactly one concise memo "
                    "or reminder sentence. Preserve dates, times, places, and objects. "
                    "Return only the sentence."
                ),
            },
            {"role": "user", "content": transcript},
        ],
    }
    data = http_json(base_url + "/chat/completions", payload, env("CHAT_API_KEY"))
    return data["choices"][0]["message"]["content"].strip()


def summarize(transcript: str) -> str:
    backend = env("CHAT_BACKEND", "rule")
    if backend == "rule":
        return summarize_rule(transcript)
    if backend == "openai_compatible":
        return summarize_openai_compatible(transcript)
    raise RuntimeError(f"Unknown CHAT_BACKEND={backend}")


class VoiceMemoHandler(BaseHTTPRequestHandler):
    server_version = "VoiceMemoGateway/0.1"

    def log_message(self, fmt: str, *args) -> None:
        print("[gateway] " + fmt % args)

    def do_GET(self) -> None:
        if self.path == "/health":
            json_response(self, 200, {"status": "ok"})
            return
        json_response(self, 404, {"error": "not found"})

    def do_POST(self) -> None:
        if self.path != "/api/voice-memo":
            json_response(self, 404, {"error": "not found"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 44:
                json_response(self, 400, {"error": "empty or invalid WAV body"})
                return
            audio = self.rfile.read(length)
            transcript = transcribe(audio)
            memo = summarize(transcript)
            json_response(self, 200, {"transcript": transcript, "memo": memo})
        except urllib.error.HTTPError as exc:
            body = exc.read().decode("utf-8", "replace")
            json_response(self, 502, {"error": f"upstream HTTP {exc.code}: {body}"})
        except Exception as exc:  # Keep device-side debugging simple.
            json_response(self, 500, {"error": str(exc)})


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()

    print(f"[gateway] listening on http://{args.host}:{args.port}")
    print(f"[gateway] STT_BACKEND={env('STT_BACKEND', 'mock')} CHAT_BACKEND={env('CHAT_BACKEND', 'rule')}")
    server = ThreadingHTTPServer((args.host, args.port), VoiceMemoHandler)
    server.serve_forever()


if __name__ == "__main__":
    main()
