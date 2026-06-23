#!/usr/bin/env python3
import os
import re
import sys


HEX_PAIR_RE = re.compile(r"\b[0-9A-Fa-f]{2}\b")


def pick_output_path(input_path: str) -> str:
    """Default output next to input, named sbc<digits>.txt when possible."""
    base = os.path.basename(input_path)
    name, _ext = os.path.splitext(base)
    digits = "".join(ch for ch in name if ch.isdigit())
    if digits:
        return os.path.join(os.path.dirname(input_path), f"sbc{digits}.txt")
    return os.path.join(os.path.dirname(input_path), "sbc_output.txt")


def parse_payload_bytes(line: str):
    """
    Parse one BLE notification line and return payload bytes after removing SN(4) + TS(4).
    Returns None when line has no usable data.
    """
    # Keep notification lines only, avoid duplicated A-lines from nRF Connect export.
    if "Notification received" not in line:
        return None
    if "(0x)" not in line:
        return None

    # Parse from the first hex byte after '(0x)'.
    try:
        hex_part = line.split("(0x)", 1)[1]
    except IndexError:
        return None

    pairs = HEX_PAIR_RE.findall(hex_part)
    if len(pairs) <= 8:
        return None

    # Remove SN(4 bytes) + TS(4 bytes)
    data = [p.upper() for p in pairs[8:]]

    # Keep SBC frames only (first byte should be SBC syncword 0x9C)
    if not data or data[0] != "9C":
        return None

    return data


def convert_log_to_sbc(input_path: str, output_path: str) -> int:
    all_bytes = []
    prev_frame = None

    with open(input_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            frame = parse_payload_bytes(line)
            if frame is None:
                continue

            # Safety dedupe for repeated adjacent exports.
            if frame == prev_frame:
                continue

            all_bytes.extend(frame)
            prev_frame = frame

    if not all_bytes:
        print("No SBC payload found in log.")
        return 2

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(" ".join(all_bytes))
        f.write("\n")

    print(f"Input : {input_path}")
    print(f"Output: {output_path}")
    print(f"Bytes : {len(all_bytes)}")
    return 0


def main() -> int:
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        print("Usage: python log_to_sbc.py <Log.txt> [output.txt]")
        return 1

    input_path = os.path.abspath(sys.argv[1])
    if not os.path.isfile(input_path):
        print(f"Input file not found: {input_path}")
        return 1

    if len(sys.argv) == 3:
        output_path = os.path.abspath(sys.argv[2])
    else:
        output_path = pick_output_path(input_path)

    return convert_log_to_sbc(input_path, output_path)


if __name__ == "__main__":
    sys.exit(main())
