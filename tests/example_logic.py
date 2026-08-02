"""Example pure logic (no hardware) — the kind of code that's easy to unit-test.

These mirror the real robot math (K230 face_x + ESP32 servo tracking),
but as plain functions so they run on any PC / in CI with no board.
"""


def compute_face_x(cx, width):
    """Face center pixel -> normalized position in [-1, 1].
    -1 = far left, 0 = center, +1 = far right.
    """
    return cx / width * 2.0 - 1.0


def face_to_speed(x, gain=35, deadzone=0.12, max_speed=60, sign=1):
    """Normalized face x -> continuous-servo speed.
    Inside the deadzone -> 0 (stop). Clamped to +/- max_speed.
    """
    if abs(x) < deadzone:
        return 0
    speed = int(sign * x * gain)
    if speed > max_speed:
        speed = max_speed
    if speed < -max_speed:
        speed = -max_speed
    return speed
