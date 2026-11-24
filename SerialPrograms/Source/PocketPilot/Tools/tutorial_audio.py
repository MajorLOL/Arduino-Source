# Sound for a tutorial film: a soft pad, a tick at each click, a chime when the flash is done.
# usage: make_audio.py events.txt out.wav
import sys, wave
import numpy as np
events = [l.split() for l in open(sys.argv[1]) if l.strip()]
end = [float(v) for k, v in events if k == "end"][0]
sr = 44100
n = int((end + 0.5) * sr)
t = np.arange(n) / sr
out = np.zeros(n)

# pad: A minor add9, slowly breathing
notes = [(110.0, 0.030), (164.81, 0.024), (220.0, 0.026), (261.63, 0.016), (329.63, 0.012)]
for i, (f, a) in enumerate(notes):
    trem = 0.7 + 0.3 * np.sin(2 * np.pi * (0.07 + 0.02 * i) * t + i)
    out += a * trem * (np.sin(2 * np.pi * f * t) + 0.35 * np.sin(2 * np.pi * f * 2.003 * t))
fade = np.minimum(1, t / 2.5) * np.minimum(1, np.maximum(0, (end - t)) / 3.0)
out *= fade

def add(start, wave_):
    i = int(start * sr)
    j = min(n, i + len(wave_))
    if i < n:
        out[i:j] += wave_[: j - i]

# tick: short, soft
tl = np.arange(int(0.12 * sr)) / sr
tick = 0.22 * np.sin(2 * np.pi * 1760 * tl) * np.exp(-tl * 55) + 0.16 * np.sin(2 * np.pi * 440 * tl) * np.exp(-tl * 40)
# chime: two notes
cl = np.arange(int(1.6 * sr)) / sr
def bell(f, a):
    return a * (np.sin(2 * np.pi * f * cl) + 0.4 * np.sin(2 * np.pi * f * 2.76 * cl)) * np.exp(-cl * 2.6) * np.minimum(1, cl / 0.01)

for k, v in events:
    v = float(v)
    if k == "click":
        add(v, tick)
    elif k == "done":
        add(v, bell(659.25, 0.16))
        add(v + 0.16, bell(880.0, 0.16))
        add(v + 0.32, bell(1318.5, 0.12))

peak = np.max(np.abs(out))
out = out / max(1.0, peak / 0.8)
pcm = (np.clip(out, -1, 1) * 32767).astype("<i2")
stereo = np.repeat(pcm[:, None], 2, axis=1)
with wave.open(sys.argv[2], "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
    w.writeframes(stereo.tobytes())
print("audio", round(end + 0.5, 1), "s")
