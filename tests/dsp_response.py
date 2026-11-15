"""Check the bass biquad's analytical response and pole stability on a PC."""

import cmath
import math


FS = 16756991 / 512  # libctru NDSP_SAMPLE_RATE
F0 = 180.0
Q = 0.8

for gain_db in (6, 9, 16, 22):
    amplitude = 10 ** (gain_db / 40)
    omega = 2 * math.pi * F0 / FS
    cosine = math.cos(omega)
    alpha = math.sin(omega) / (2 * Q)
    a0, a1, a2 = 1 + alpha / amplitude, -2 * cosine, 1 - alpha / amplitude
    b0, b1, b2 = 1 + alpha * amplitude, -2 * cosine, 1 - alpha * amplitude
    roots = (
        (-a1 + cmath.sqrt(a1 * a1 - 4 * a0 * a2)) / (2 * a0),
        (-a1 - cmath.sqrt(a1 * a1 - 4 * a0 * a2)) / (2 * a0),
    )
    peak_pole = max(abs(root) for root in roots)
    z_inv = cmath.exp(-1j * omega)
    response = (b0 + b1 * z_inv + b2 * z_inv**2) / (a0 + a1 * z_inv + a2 * z_inv**2)
    measured_db = 20 * math.log10(abs(response))
    assert peak_pole < 1, (gain_db, peak_pole)
    assert abs(measured_db - gain_db) < 1e-9, (gain_db, measured_db)
    print(f"+{gain_db:2d} dB: 180 Hz = {measured_db:.3f} dB, max pole radius = {peak_pole:.6f}")
