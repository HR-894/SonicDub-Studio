import argparse
import os
import threading
import sys

from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn


app = FastAPI(title="SonicDub AI Bridge")


class TextPayload(BaseModel):
    text: str
    language: str
    reference_audio_path: str
    output_dir: str
    segment_id: int  # Unique per-segment — prevents hash collision on identical text


class DiarizePayload(BaseModel):
    audio_path: str


@app.post("/diarize")
async def diarize_audio(payload: DiarizePayload):
    """
    Ingests a WAV file path and returns time-coded speaker segments.
    """
    mock_segments = [
        {"start_ms": 0, "end_ms": 3500, "speaker": "SPEAKER_00"},
        {"start_ms": 3600, "end_ms": 8000, "speaker": "SPEAKER_01"},
    ]
    return {"message": "success", "segments": mock_segments}


@app.post("/clone_tts")
async def clone_tts(payload: TextPayload):
    """
    Ingests text, language, reference WAV, and segment ID.
    Uses segment_id for the filename to guarantee uniqueness
    even when two segments contain identical text.
    """
    output_path = os.path.join(
        payload.output_dir,
        f"xtts_seg_{payload.segment_id}.wav",
    )

    # In production:
    #   from TTS.api import TTS
    #   tts = TTS("tts_models/multilingual/multi-dataset/xtts_v2")
    #   tts.tts_to_file(text=payload.text, language=payload.language,
    #                   speaker_wav=payload.reference_audio_path,
    #                   file_path=output_path)

    # MOCK
    with open(output_path, "wb") as f:
        f.write(b"mock_wav_data")

    return {"message": "success", "output_path": output_path}


def _watchdog(parent_pid: int):
    """
    Background thread that polls whether the C++ parent process is still alive.
    If the parent is force-killed (Task Manager, segfault), this thread
    detects it and immediately terminates the Python process so it doesn't
    become a zombie hogging 1GB+ of PyTorch RAM.
    """
    import time

    while True:
        time.sleep(2)
        try:
            # os.kill with signal 0 checks existence without killing
            os.kill(parent_pid, 0)
        except OSError:
            # Parent is dead — self-destruct
            print(
                f"[AI Bridge] Parent PID {parent_pid} exited. "
                "Shutting down.",
                file=sys.stderr,
            )
            os._exit(0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="SonicDub AI Bridge Server"
    )
    parser.add_argument(
        "--port", type=int, default=0,
        help="Port to bind to (0 = let OS choose a free port)",
    )
    parser.add_argument(
        "--parent-pid", type=int, default=0,
        help="PID of the C++ parent process for zombie prevention",
    )
    parser.add_argument(
        "--port-file", type=str, default="",
        help="File to write the actual bound port number to",
    )
    args = parser.parse_args()

    # Start zombie watchdog if parent PID was provided
    if args.parent_pid > 0:
        t = threading.Thread(
            target=_watchdog, args=(args.parent_pid,), daemon=True
        )
        t.start()

    # Use a custom uvicorn server so we can discover the actual port
    config = uvicorn.Config(
        app, host="127.0.0.1", port=args.port, log_level="info"
    )
    server = uvicorn.Server(config)

    # After binding, write the real port to the port file so C++ can read it
    import asyncio

    async def _serve_and_report():
        config.setup_event_loop()
        server.config.load()
        server.lifespan = server.config.lifespan_class(server.config)
        await server.startup()
        if server.started and args.port_file:
            # Extract the actual port from the bound sockets
            for sock in server.servers:
                for s in sock.sockets:
                    actual_port = s.getsockname()[1]
                    with open(args.port_file, "w") as pf:
                        pf.write(str(actual_port))
                    break
                break
        await server.main_loop()
        await server.shutdown()

    asyncio.run(_serve_and_report())
