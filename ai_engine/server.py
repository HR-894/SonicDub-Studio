import argparse
import os

from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn


app = FastAPI(title="SonicDub AI Bridge")


class TextPayload(BaseModel):
    text: str
    language: str
    reference_audio_path: str
    output_dir: str  # C++ temp_dir — files go here for automatic cleanup


class DiarizePayload(BaseModel):
    audio_path: str


@app.post("/diarize")
async def diarize_audio(payload: DiarizePayload):
    """
    Ingests a WAV file path and returns time-coded speaker segments.
    (Integration of PyAnnote/WhisperX logic will go here).
    """
    # MOCK implementation for Phase 1 of AI bridge.
    # Return structure expected by C++ Diarizer stage.
    mock_segments = [
        {"start_ms": 0, "end_ms": 3500, "speaker": "SPEAKER_00"},
        {"start_ms": 3600, "end_ms": 8000, "speaker": "SPEAKER_01"}
    ]

    return {"message": "success", "segments": mock_segments}


@app.post("/clone_tts")
async def clone_tts(payload: TextPayload):
    """
    Ingests text, language, and a reference WAV of the speaker,
    then uses Coqui XTTSv2 to synthesize and clone the audio dynamically.
    """
    # Build output path inside the C++ job's temp directory so cleanup is automatic.
    # Use abs(hash()) to avoid negative signs in filenames on Windows.
    output_path = os.path.join(
        payload.output_dir,
        f"xtts_{abs(hash(payload.text))}.wav"
    )

    # In production, this will invoke:
    #   from TTS.api import TTS
    #   tts = TTS("tts_models/multilingual/multi-dataset/xtts_v2")
    #   tts.tts_to_file(text=payload.text, language=payload.language,
    #                   speaker_wav=payload.reference_audio_path, file_path=output_path)

    # MOCK: touch a dummy file so the C++ side can read it
    with open(output_path, "wb") as f:
        f.write(b"mock_wav_data")

    return {"message": "success", "output_path": output_path}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="SonicDub AI Bridge Server")
    parser.add_argument("--port", type=int, default=8000,
                        help="Port to bind the server to")
    args = parser.parse_args()

    uvicorn.run(app, host="127.0.0.1", port=args.port)
