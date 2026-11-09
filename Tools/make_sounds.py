"""Generates Pac Shark's sound effects into Art/ as 16-bit mono 44.1 kHz .wav files.

Run: python Tools/make_sounds.py   (needs numpy)
Replace any Sfx_*.wav with your own recording of the same format to change a sound.
"""
import os
import wave

import numpy as np

RATE = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Art")


def t_axis(seconds):
    return np.arange(int(RATE * seconds)) / RATE


def sweep(f0, f1, seconds, shape="sine"):
    t = t_axis(seconds)
    freq = f0 * (f1 / f0) ** (t / seconds)  # exponential glide
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    if shape == "square":
        return np.sign(np.sin(phase)) * 0.5 + np.sin(phase) * 0.5
    if shape == "tri":
        return 2 / np.pi * np.arcsin(np.sin(phase))
    return np.sin(phase)


def envelope(n, attack=0.005, release=0.05):
    env = np.ones(n)
    a = max(1, int(RATE * attack))
    r = max(1, int(RATE * release))
    env[:a] = np.linspace(0, 1, a)
    env[-r:] *= np.linspace(1, 0, r)
    return env


def bubble(seconds, f0, f1, wobble=0.0, wobble_rate=0.0):
    t = t_axis(seconds)
    freq = f0 * (f1 / f0) ** (t / seconds)
    if wobble:
        freq = freq * (1 + wobble * np.sin(2 * np.pi * wobble_rate * t))
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    return np.sin(phase) * np.exp(-t * 3 / seconds)


def save(name, samples, volume=0.8):
    samples = samples / max(1e-9, np.max(np.abs(samples))) * volume
    data = (samples * 32767).astype("<i2").tobytes()
    with wave.open(os.path.join(OUT, name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)


def concat(*parts):
    return np.concatenate(parts)


def main():
    # Chomp: a short watery blip, played for every dot (pitch alternates in game).
    chomp = bubble(0.075, 420, 900) * 0.7 + sweep(300, 160, 0.075, "tri") * 0.3
    save("Sfx_Chomp.wav", chomp * envelope(len(chomp), 0.002, 0.02), 0.55)

    # Power dot: a rising rush of bubbles.
    t = t_axis(0.6)
    power = sweep(180, 1200, 0.6, "tri") * 0.5
    for i, start in enumerate(np.linspace(0, 0.45, 7)):
        b = bubble(0.12, 500 + i * 120, 1400 + i * 150)
        s = int(start * RATE)
        power[s:s + len(b)] += b * 0.5
    save("Sfx_Power.wav", power * envelope(len(power), 0.01, 0.15), 0.7)

    # Eating an octopus: a quick gulp that swoops up.
    gulp = concat(sweep(700, 200, 0.08, "square") * 0.6, sweep(250, 1600, 0.22, "tri"))
    save("Sfx_EatOctopus.wav", gulp * envelope(len(gulp), 0.003, 0.06), 0.7)

    # Shark dies: a slow, wobbling slide down, then a couple of bubbles floating up.
    death = sweep(900, 90, 1.2, "tri") * (1 + 0.3 * np.sin(2 * np.pi * 9 * t_axis(1.2)))
    tail = np.zeros(int(RATE * 0.5))
    for start, f in ((0.05, 600), (0.25, 900)):
        b = bubble(0.15, f, f * 2.2)
        s = int(start * RATE)
        tail[s:s + len(b)] += b
    death = concat(death * envelope(len(death), 0.01, 0.2), tail * 0.5)
    save("Sfx_Death.wav", death, 0.7)

    # Level clear: a bright little arpeggio.
    notes = [523.25, 659.25, 783.99, 1046.5]
    clear = concat(*[sweep(f, f, 0.14, "tri") * envelope(int(RATE * 0.14), 0.005, 0.05) for f in notes],
                   sweep(1046.5, 1046.5, 0.35, "tri") * envelope(int(RATE * 0.35), 0.005, 0.3))
    save("Sfx_LevelClear.wav", clear, 0.6)

    # Extra life: a two-note chime.
    chime = concat(*[(sweep(f, f, 0.22) + 0.3 * sweep(f * 2, f * 2, 0.22)) * envelope(int(RATE * 0.22), 0.003, 0.18)
                     for f in (880, 1318.5)])
    save("Sfx_ExtraLife.wav", chime, 0.6)

    # Menu move / confirm clicks.
    tick = bubble(0.04, 1200, 1500)
    save("Sfx_MenuMove.wav", tick * envelope(len(tick), 0.001, 0.01), 0.4)
    ok = concat(bubble(0.05, 800, 1000), bubble(0.08, 1200, 1700))
    save("Sfx_MenuSelect.wav", ok * envelope(len(ok), 0.001, 0.02), 0.5)


if __name__ == "__main__":
    main()
