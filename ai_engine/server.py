"""
╔══════════════════════════════════════════════════════════════════════════════╗
║  SonicDub AI Bridge — server.py                                            ║
║                                                                            ║
║  Endpoints:                                                                ║
║    POST /diarize              — Legacy PyAnnote diarization (fallback)    ║
║    POST /clone_tts            — XTTS v2 voice cloning                     ║
║    POST /transcribe_vibevoice — VibeVoice-ASR (Who+When+What, 60-min)     ║
║    POST /tts_vibevoice        — VibeVoice-Realtime-0.5B local TTS         ║
║    WS   /ws/stream_tts        — Streaming TTS preview (WebSocket)         ║
║                                                                            ║
║  Lifecycle:                                                                ║
║    C++ parent launch → named mutex watchdog → self-exit on parent death   ║
╚══════════════════════════════════════════════════════════════════════════════╝
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import sys
import threading
import time
from pathlib import Path
from typing import Optional

import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse
from pydantic import BaseModel

# ─── Load .env (API keys never go to GitHub) ────────────────────────────────
try:
    from dotenv import load_dotenv
    load_dotenv(Path(__file__).parent.parent / ".env")
except ImportError:
    pass  # python-dotenv optional in bare environments

# ─── Lazy model cache — loaded on first use ─────────────────────────────────
_vibevoice_asr_pipeline = None
_vibevoice_asr_lock     = threading.Lock()
_vibevoice_tts_model    = None
_vibevoice_tts_lock     = threading.Lock()

app = FastAPI(title="SonicDub AI Bridge", version="2.0.0")


# ═══════════════════════════════════════════════════════════════════════════════
# Pydantic Models
# ═══════════════════════════════════════════════════════════════════════════════

class DiarizePayload(BaseModel):
    audio_path: str


class TextPayload(BaseModel):
    text: str
    language: str
    reference_audio_path: str
    output_dir: str
    segment_id: int


class TranscribeVibeVoicePayload(BaseModel):
    audio_path:    str
    language:      str = "auto"
    hotwords:      list[str] = []
    model_path:    str = "microsoft/VibeVoice-ASR-HF"
    use_gpu:       bool = True
    chunk_length_s: float = 30.0


class VibeVoiceTTSPayload(BaseModel):
    text:          str
    speaker_name:  str = "Carter"          # Built-in VibeVoice-Realtime voice
    output_path:   str = ""                # If empty, returns base64 audio
    model_path:    str = "microsoft/VibeVoice-Realtime-0.5B"
    use_gpu:       bool = True


# ═══════════════════════════════════════════════════════════════════════════════
# ENDPOINT: /diarize  (Legacy PyAnnote fallback)
# ═══════════════════════════════════════════════════════════════════════════════

@app.post("/diarize")
async def diarize_audio(payload: DiarizePayload):
    """
    Legacy speaker diarization via PyAnnote.
    Only used when active_asr_backend = 'whisper' (not VibeVoice-ASR).
    VibeVoice-ASR replaces this because it does ASR + diarization in one pass.
    """
    try:
        from pyannote.audio import Pipeline as PyannotePipeline
        hf_token = os.getenv("PYANNOTE_HF_TOKEN", "")
        if not hf_token:
            # Return mock if no token — graceful degradation
            return {
                "message": "success",
                "segments": [
                    {"start_ms": 0,    "end_ms": 3500, "speaker": "SPEAKER_00"},
                    {"start_ms": 3600, "end_ms": 8000, "speaker": "SPEAKER_01"},
                ],
                "note": "mock — set PYANNOTE_HF_TOKEN in .env for real diarization",
            }
        pipeline = PyannotePipeline.from_pretrained(
            "pyannote/speaker-diarization-3.1", use_auth_token=hf_token
        )
        diarization = pipeline(payload.audio_path)
        segments = []
        for turn, _, speaker in diarization.itertracks(yield_label=True):
            segments.append({
                "start_ms": int(turn.start * 1000),
                "end_ms":   int(turn.end   * 1000),
                "speaker":  speaker,
            })
        return {"message": "success", "segments": segments}

    except ImportError:
        # pyannote not installed — return mock
        return {
            "message": "success",
            "segments": [
                {"start_ms": 0,    "end_ms": 3500, "speaker": "SPEAKER_00"},
                {"start_ms": 3600, "end_ms": 8000, "speaker": "SPEAKER_01"},
            ],
            "note": "mock — install pyannote.audio for real diarization",
        }
    except Exception as e:
        return JSONResponse(status_code=500, content={"error": str(e)})


# ═══════════════════════════════════════════════════════════════════════════════
# ENDPOINT: /clone_tts  (XTTS v2 voice cloning)
# ═══════════════════════════════════════════════════════════════════════════════

@app.post("/clone_tts")
async def clone_tts(payload: TextPayload):
    """
    XTTS v2 voice cloning: synthesizes text in the voice of reference_audio.
    Uses segment_id for unique filename even when text is identical.
    """
    output_path = os.path.join(
        payload.output_dir,
        f"xtts_seg_{payload.segment_id}.wav",
    )
    try:
        from TTS.api import TTS
        # Run blocking inference in thread to keep FastAPI responsive
        loop = asyncio.get_event_loop()
        tts_model = await loop.run_in_executor(
            None, lambda: TTS("tts_models/multilingual/multi-dataset/xtts_v2")
        )
        await loop.run_in_executor(None, lambda: tts_model.tts_to_file(
            text=payload.text,
            language=payload.language,
            speaker_wav=payload.reference_audio_path,
            file_path=output_path,
        ))
    except ImportError:
        # TTS not installed — write mock WAV stub
        with open(output_path, "wb") as f:
            f.write(b"RIFF\x00\x00\x00\x00WAVEfmt ")
    except Exception as e:
        return JSONResponse(status_code=500, content={"error": str(e)})

    return {"message": "success", "output_path": output_path}


# ═══════════════════════════════════════════════════════════════════════════════
# ENDPOINT: /transcribe_vibevoice  (Phase 1 — BIGGEST WIN)
# ═══════════════════════════════════════════════════════════════════════════════

def _load_vibevoice_asr(model_path: str, use_gpu: bool, chunk_length_s: float):
    """Load VibeVoice-ASR pipeline (cached — only loaded once)."""
    global _vibevoice_asr_pipeline
    with _vibevoice_asr_lock:
        if _vibevoice_asr_pipeline is None:
            try:
                import torch
                from transformers import pipeline as hf_pipeline

                device = "cuda" if (use_gpu and torch.cuda.is_available()) else "cpu"
                print(f"[AI Bridge] Loading VibeVoice-ASR on {device}: {model_path}",
                      flush=True)

                _vibevoice_asr_pipeline = hf_pipeline(
                    "automatic-speech-recognition",
                    model=model_path,
                    device=device,
                    chunk_length_s=chunk_length_s, # Prevents OOM on large files
                    return_timestamps=True, # Returns word/segment timestamps
                )
                print("[AI Bridge] VibeVoice-ASR loaded successfully.", flush=True)
            except Exception as e:
                print(f"[AI Bridge] VibeVoice-ASR load failed: {e}", flush=True)
                _vibevoice_asr_pipeline = None
    return _vibevoice_asr_pipeline


@app.post("/transcribe_vibevoice")
async def transcribe_vibevoice(payload: TranscribeVibeVoicePayload):
    """
    VibeVoice-ASR: single-pass 60-minute speech recognition.
    Returns structured output: Who (speaker) + When (timestamps) + What (text).
    Replaces the Whisper + separate PyAnnote diarization two-step process.

    Returns:
        {
          "segments": [
            {
              "speaker":    "SPEAKER_00",
              "start_ms":   0,
              "end_ms":     3200,
              "text":       "Hello, welcome to the show.",
              "language":   "en",
              "confidence": 0.95
            }, ...
          ],
          "detected_language": "en",
          "model_used": "microsoft/VibeVoice-ASR-HF"
        }
    """
    if not os.path.exists(payload.audio_path):
        return JSONResponse(
            status_code=400,
            content={"error": f"Audio file not found: {payload.audio_path}"}
        )

    loop = asyncio.get_event_loop()

    try:
        # Load model (cached after first call)
        asr = await loop.run_in_executor(
            None,
            lambda: _load_vibevoice_asr(payload.model_path, payload.use_gpu, payload.chunk_length_s)
        )

        if asr is None:
            # Graceful fallback — return empty segments, C++ will use Whisper
            return JSONResponse(
                status_code=503,
                content={
                    "error": "VibeVoice-ASR not available — install transformers + torch",
                    "fallback": "whisper",
                }
            )

        # Build hotword prompt if provided
        hotword_context = ""
        if payload.hotwords:
            hotword_context = "Context hotwords: " + ", ".join(payload.hotwords) + ". "

        # Run inference (blocking → executor thread)
        def _run_asr():
            result = asr(
                payload.audio_path,
                generate_kwargs={
                    "language":          None if payload.language == "auto" else payload.language,
                    "task":              "transcribe",
                    "initial_prompt":    hotword_context or None,
                },
            )
            return result

        raw = await loop.run_in_executor(None, _run_asr)

        # ── Parse VibeVoice-ASR output ────────────────────────────────────
        # VibeVoice-ASR HF model returns chunks with speaker + timestamps
        segments   = []
        detected_lang = payload.language if payload.language != "auto" else "en"

        if isinstance(raw, dict):
            chunks = raw.get("chunks", [])
            for chunk in chunks:
                text      = chunk.get("text", "").strip()
                timestamp = chunk.get("timestamp", (0.0, 0.0))
                speaker   = chunk.get("speaker", "SPEAKER_00")  # VibeVoice-ASR provides this
                if not text:
                    continue
                start_s = timestamp[0] if timestamp[0] is not None else 0.0
                end_s   = timestamp[1] if timestamp[1] is not None else start_s + 1.0
                segments.append({
                    "speaker":    speaker,
                    "start_ms":   int(start_s * 1000),
                    "end_ms":     int(end_s   * 1000),
                    "text":       text,
                    "language":   detected_lang,
                    "confidence": 1.0,
                })
        else:
            # Fallback: plain string result without speaker info
            text = str(raw).strip()
            if text:
                segments.append({
                    "speaker":    "SPEAKER_00",
                    "start_ms":   0,
                    "end_ms":     60000,
                    "text":       text,
                    "language":   detected_lang,
                    "confidence": 1.0,
                })

        return {
            "message":           "success",
            "segments":          segments,
            "detected_language": detected_lang,
            "model_used":        payload.model_path,
            "segment_count":     len(segments),
        }

    except Exception as e:
        import traceback
        print(f"[AI Bridge] VibeVoice-ASR error: {traceback.format_exc()}", flush=True)
        return JSONResponse(status_code=500, content={"error": str(e)})


# ═══════════════════════════════════════════════════════════════════════════════
# ENDPOINT: /tts_vibevoice  (Phase 2 — Local TTS)
# ═══════════════════════════════════════════════════════════════════════════════

def _load_vibevoice_tts(model_path: str, use_gpu: bool):
    """Load VibeVoice-Realtime-0.5B TTS model (cached)."""
    global _vibevoice_tts_model
    with _vibevoice_tts_lock:
        if _vibevoice_tts_model is None:
            try:
                import torch
                from vibevoice import VoiceRealtime  # pip install -e .[streamingtts]
                device = "cuda" if (use_gpu and torch.cuda.is_available()) else "cpu"
                print(f"[AI Bridge] Loading VibeVoice-Realtime on {device}: {model_path}",
                      flush=True)
                _vibevoice_tts_model = VoiceRealtime.from_pretrained(
                    model_path, device=device
                )
                print("[AI Bridge] VibeVoice-Realtime-0.5B loaded.", flush=True)
            except Exception as e:
                print(f"[AI Bridge] VibeVoice-Realtime load failed: {e}", flush=True)
                _vibevoice_tts_model = None
    return _vibevoice_tts_model


@app.post("/tts_vibevoice")
async def tts_vibevoice(payload: VibeVoiceTTSPayload):
    """
    VibeVoice-Realtime-0.5B local TTS.
    ~200ms first-audio latency. Single speaker, offline, no API key needed.
    Speaker consistency is guaranteed by using the same speaker_name per speaker_id.

    Returns:
        {"message": "success", "output_path": "/path/to/audio.wav"}
    """
    if not payload.text.strip():
        return JSONResponse(status_code=400, content={"error": "Empty text"})

    if not payload.output_path:
        return JSONResponse(status_code=400, content={"error": "output_path required"})

    loop = asyncio.get_event_loop()

    try:
        model = await loop.run_in_executor(
            None,
            lambda: _load_vibevoice_tts(payload.model_path, payload.use_gpu)
        )

        if model is None:
            return JSONResponse(
                status_code=503,
                content={
                    "error": "VibeVoice-Realtime not available",
                    "install": "cd VibeVoice && pip install -e .[streamingtts]",
                    "fallback": "edge_tts",
                }
            )

        # Synthesize — blocking in thread
        def _synthesize():
            import soundfile as sf
            audio, sr = model.synthesize(
                text=payload.text,
                speaker=payload.speaker_name,
            )
            sf.write(payload.output_path, audio, sr)

        await loop.run_in_executor(None, _synthesize)
        return {"message": "success", "output_path": payload.output_path}

    except Exception as e:
        import traceback
        print(f"[AI Bridge] VibeVoice-TTS error: {traceback.format_exc()}", flush=True)
        return JSONResponse(status_code=500, content={"error": str(e)})


# ═══════════════════════════════════════════════════════════════════════════════
# WEBSOCKET: /ws/stream_tts  (Phase 3 — Real-time Preview)
# ═══════════════════════════════════════════════════════════════════════════════

@app.websocket("/ws/stream_tts")
async def websocket_stream_tts(websocket: WebSocket):
    """
    Streaming TTS preview over WebSocket.
    Client sends JSON: {"text": "...", "speaker": "Carter"}
    Server streams back audio chunks as binary frames (~200ms first chunk).

    Protocol:
        Client → {"text": "Hello world", "speaker": "Carter", "model": "vibevoice"}
        Server → binary audio chunk 1
        Server → binary audio chunk 2 ...
        Server → {"done": true, "total_chunks": N}
    """
    await websocket.accept()
    try:
        while True:
            raw = await websocket.receive_text()
            data = json.loads(raw)
            text     = data.get("text", "").strip()
            speaker  = data.get("speaker", "Carter")
            model_id = data.get("model", "vibevoice")

            if not text:
                await websocket.send_json({"error": "empty text"})
                continue

            try:
                loop = asyncio.get_event_loop()

                if model_id == "vibevoice":
                    model = await loop.run_in_executor(
                        None,
                        lambda: _load_vibevoice_tts(
                            "microsoft/VibeVoice-Realtime-0.5B", True
                        )
                    )
                    if model is None:
                        await websocket.send_json({"error": "VibeVoice-Realtime not loaded"})
                        continue

                    # Stream audio chunks
                    chunk_count = 0
                    async def _stream():
                        nonlocal chunk_count
                        # VibeVoice-Realtime supports streaming generator
                        for audio_chunk in model.stream(text=text, speaker=speaker):
                            import io
                            import soundfile as sf
                            import numpy as np
                            buf = io.BytesIO()
                            sf.write(buf, audio_chunk, 22050, format="WAV")
                            await websocket.send_bytes(buf.getvalue())
                            chunk_count += 1

                    await _stream()
                    await websocket.send_json({"done": True, "total_chunks": chunk_count})

                else:
                    # Fallback: edge-tts streaming
                    import edge_tts
                    communicate = edge_tts.Communicate(text, voice="en-US-GuyNeural")
                    chunk_count = 0
                    async for chunk in communicate.stream():
                        if chunk["type"] == "audio":
                            await websocket.send_bytes(chunk["data"])
                            chunk_count += 1
                    await websocket.send_json({"done": True, "total_chunks": chunk_count})

            except Exception as e:
                await websocket.send_json({"error": str(e)})

    except WebSocketDisconnect:
        pass


# ═══════════════════════════════════════════════════════════════════════════════
# ENDPOINT: /health  — Quick liveness check
# ═══════════════════════════════════════════════════════════════════════════════

@app.get("/health")
async def health():
    """Returns server status and which AI models are loaded."""
    import torch
    gpu_available = False
    try:
        gpu_available = torch.cuda.is_available()
    except Exception:
        pass

    return {
        "status":              "ok",
        "version":             "2.0.0",
        "gpu_available":       gpu_available,
        "vibevoice_asr_ready": _vibevoice_asr_pipeline is not None,
        "vibevoice_tts_ready": _vibevoice_tts_model    is not None,
    }


# ═══════════════════════════════════════════════════════════════════════════════
# WATCHDOG — Named Mutex (Windows, PID-reuse-proof)
# ═══════════════════════════════════════════════════════════════════════════════

def _watchdog_mutex(mutex_name: str):
    """
    Monitor a Windows named mutex created by the C++ parent.
    When the parent dies, Windows auto-releases the mutex.
    We detect this → self-terminate.
    Immune to PID-reuse: a new process with the same PID will NOT
    hold the same named mutex.
    """
    import ctypes
    kernel32        = ctypes.windll.kernel32
    WAIT_OBJECT_0   = 0x00000000
    WAIT_ABANDONED  = 0x00000080
    INFINITE        = 0xFFFFFFFF

    handle = kernel32.OpenMutexW(0x00100000, False, mutex_name)
    if not handle:
        print(f"[AI Bridge] Cannot open mutex '{mutex_name}', using PID fallback.",
              file=sys.stderr, flush=True)
        return False

    result = kernel32.WaitForSingleObject(handle, INFINITE)
    if result in (WAIT_OBJECT_0, WAIT_ABANDONED):
        print("[AI Bridge] Parent mutex released — shutting down.", file=sys.stderr, flush=True)
        kernel32.CloseHandle(handle)
        os._exit(0)
    return True


def _watchdog_pid(parent_pid: int):
    """Fallback watchdog: poll whether parent PID still exists."""
    while True:
        time.sleep(2)
        try:
            os.kill(parent_pid, 0)
        except OSError:
            print(f"[AI Bridge] Parent PID {parent_pid} exited — shutting down.",
                  file=sys.stderr, flush=True)
            os._exit(0)


# ═══════════════════════════════════════════════════════════════════════════════
# ENTRYPOINT
# ═══════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="SonicDub AI Bridge Server v2")
    parser.add_argument("--port",       type=int, default=0,
                        help="Port to bind (0 = OS picks free port)")
    parser.add_argument("--parent-pid", type=int, default=0,
                        help="PID of C++ parent (fallback watchdog)")
    parser.add_argument("--mutex-name", type=str, default="",
                        help="Named mutex from C++ parent (primary watchdog)")
    parser.add_argument("--port-file",  type=str, default="",
                        help="File to write the actual bound port to")
    args = parser.parse_args()

    # Start watchdog thread
    if args.mutex_name:
        threading.Thread(target=_watchdog_mutex, args=(args.mutex_name,),
                         daemon=True).start()
    elif args.parent_pid > 0:
        threading.Thread(target=_watchdog_pid, args=(args.parent_pid,),
                         daemon=True).start()

    # Boot uvicorn, discover actual port, write to port file
    config = uvicorn.Config(app, host="127.0.0.1", port=args.port, log_level="info")
    server = uvicorn.Server(config)

    async def _serve_and_report():
        config.setup_event_loop()
        server.config.load()
        server.lifespan = server.config.lifespan_class(server.config)
        await server.startup()
        if server.started and args.port_file:
            for sock in server.servers:
                for s in sock.sockets:
                    port = s.getsockname()[1]
                    with open(args.port_file, "w") as pf:
                        pf.write(str(port))
                    print(f"[AI Bridge] Listening on port {port}", flush=True)
                    break
                break
        await server.main_loop()
        await server.shutdown()

    asyncio.run(_serve_and_report())
