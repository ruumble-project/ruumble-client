#!/usr/bin/env python3
# Writes the push-to-talk cues of Ruumble Client (#9) into client/sounds: a short click when the mic opens (1200 Hz)
# and a lower one when it closes (800 Hz), 35 ms each, at -20 dBFS. Chosen by ear from three variants; run again only
# to change them.
import math
import struct
import wave
from pathlib import Path

SR = 48000


def click(freq, ms, db):
    amp = 10 ** (db / 20) * 32767
    n = int(SR * ms / 1000)
    out = []
    for i in range(n):
        env = min(1.0, i / (0.003 * SR)) * math.exp(-4.0 * i / n)  # 3 ms attack, then decay
        s = math.sin(2 * math.pi * freq * i / SR) + 0.15 * math.sin(4 * math.pi * freq * i / SR)
        out.append(int(amp * env * s / 1.15))
    return out + [0] * int(SR * 0.004)


def save(path, samples):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", s) for s in samples))


out = Path(__file__).resolve().parent.parent / "client" / "sounds"
out.mkdir(exist_ok=True)
save(out / "ptt-on.wav", click(1200, 35, -20))
save(out / "ptt-off.wav", click(800, 35, -20))
print(f"written to {out}")
