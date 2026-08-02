"""Unit tests for example_logic.

How it works:
- pytest auto-finds files named test_*.py and functions named test_*.
- Each test calls the function and `assert`s the result equals what we expect.
- If any assert is False -> the test FAILS (red). All pass -> green.
Run locally:  pytest tests/ -v
"""

from example_logic import compute_face_x, face_to_speed


# ── compute_face_x: pixel -> normalized [-1, 1] ──────────────────
def test_center_is_zero():
    assert compute_face_x(400, 800) == 0.0        # middle of 800px = 0

def test_left_edge_is_minus_one():
    assert compute_face_x(0, 800) == -1.0         # far left = -1

def test_right_edge_is_plus_one():
    assert compute_face_x(800, 800) == 1.0        # far right = +1


# ── face_to_speed: face position -> servo speed ──────────────────
def test_deadzone_stops():
    assert face_to_speed(0.05) == 0               # near center -> stop

def test_right_is_positive():
    assert face_to_speed(0.8) > 0                 # face right -> turn one way

def test_left_is_negative():
    assert face_to_speed(-0.8) < 0                # face left -> the other way

def test_speed_is_clamped():
    assert face_to_speed(1.0, gain=1000) == 60    # never exceeds max_speed

def test_sign_flips_direction():
    assert face_to_speed(0.8, sign=-1) < 0        # sign inverts direction
