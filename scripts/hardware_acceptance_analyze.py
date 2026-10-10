"""Check physical input logs against the instructed one-at-a-time order."""

import argparse
import pathlib
import re


MATRIX = re.compile(r"MATRIX row=(\d+) col=([A-I]) state=(DOWN|UP) count=(\d+)")
WAKE = re.compile(r"WAKE input=([1-4]) state=(ACTIVE|RELEASED) count=(\d+)")


def matrix(raw):
    observed = [
        (int(row), col, state)
        for row, col, state, _ in MATRIX.findall(raw)
    ]
    expected = [
        (row, col, state)
        for row in range(1, 10)
        for col in "ABCDEFGHI"
        for state in ("DOWN", "UP")
    ]
    first_difference = next(
        (index for index, (actual, wanted) in enumerate(zip(observed, expected))
         if actual != wanted),
        min(len(observed), len(expected)) if len(observed) != len(expected) else None,
    )
    print("matrix_events={} expected={} exact_row_major={}".format(
        len(observed), len(expected), observed == expected
    ))
    if first_difference is not None:
        print("first_difference={} actual={} expected={}".format(
            first_difference,
            observed[first_difference] if first_difference < len(observed) else "MISSING",
            expected[first_difference] if first_difference < len(expected) else "EXTRA",
        ))
    return observed == expected


def wake(raw):
    observed = [
        (int(number), state)
        for number, state, _ in WAKE.findall(raw)
    ]
    expected = [
        (number, state)
        for number in range(1, 5)
        for _ in range(3)
        for state in ("ACTIVE", "RELEASED")
    ]
    print("wake_events={} expected={} exact_single_order={}".format(
        len(observed), len(expected), observed == expected
    ))
    for number in range(1, 5):
        active = sum(item == (number, "ACTIVE") for item in observed)
        released = sum(item == (number, "RELEASED") for item in observed)
        print("input={} active={} released={}".format(number, active, released))
    return observed == expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("wake", "matrix"))
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    raw = args.log.read_text(encoding="utf-8", errors="replace")
    ok = matrix(raw) if args.kind == "matrix" else wake(raw)
    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    main()
