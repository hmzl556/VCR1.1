import argparse
import os
import subprocess
import tempfile


FRAME_HEADER = [0x9C, 0x31, 0x19]
EXPECTED_FRAME_LEN = 58


def analyze_frame_lengths(data_bytes):
    header_len = len(FRAME_HEADER)
    starts = []
    for index in range(0, len(data_bytes) - header_len + 1):
        if list(data_bytes[index:index + header_len]) == FRAME_HEADER:
            starts.append(index)

    if not starts:
        return None

    lengths = []
    for index, start in enumerate(starts):
        if index + 1 < len(starts):
            lengths.append(starts[index + 1] - start)
        else:
            lengths.append(len(data_bytes) - start)

    bad_lengths = [length for length in lengths if length != EXPECTED_FRAME_LEN]
    return {
        "frame_count": len(starts),
        "min_len": min(lengths),
        "max_len": max(lengths),
        "bad_count": len(bad_lengths),
    }

def convert_txt_hex_to_sbc(input_file, output_sbc):
    print(f"Converting spaced hex {input_file} to {output_sbc}...")
    try:
        all_bytes = bytearray()
        with open(input_file, 'r') as f_in, open(output_sbc, 'wb') as f_out:
            started = False
            prev = []
            dropped = 0
            for line in f_in:
                tokens = line.strip().split()
                if not tokens:
                    continue
                for token in tokens:
                    if len(token) != 2:
                        continue
                    try:
                        value = int(token, 16)
                        all_bytes.append(value)
                        if not started:
                            dropped += 1
                            prev.append(value)
                            if len(prev) > 3:
                                prev.pop(0)
                            if prev == [0x9C, 0x31, 0x19]:
                                started = True
                                if dropped > 3:
                                    print("Successfully cleared leading bad data.")
                                f_out.write(bytes(prev))
                            continue
                        f_out.write(bytes([value]))
                    except ValueError:
                        continue
        stats = analyze_frame_lengths(all_bytes)
        if stats is None:
            print("No SBC frame header 9C 31 19 found.")
            return False
        print(
            f"Frame stats: frames={stats['frame_count']}, "
            f"min_len={stats['min_len']}, max_len={stats['max_len']}, "
            f"bad_len_frames={stats['bad_count']} (expected {EXPECTED_FRAME_LEN})"
        )
        print("Conversion to .sbc finished.")
        return True
    except Exception as e:
        print(f"An error occurred: {e}")
        return False

def convert_sbc_to_wav(input_sbc, output_wav):
    print(f"Converting {input_sbc} to {output_wav} using ffmpeg...")
    # 注意：SBC 是原始流，ffmpeg 可能需要显式指定格式或尝试自动识别
    # 如果 ffmpeg 无法识别，请确保安装了支持 sbc 的 ffmpeg 版本
    command = [
        'ffmpeg',
        '-y',               # 覆盖输出文件
        '-i', input_sbc,    # 输入文件
        output_wav          # 输出文件
    ]
    
    try:
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"Successfully created {output_wav}")
        else:
            print("ffmpeg error:")
            print(result.stderr)
            print("\nTrying with explicit sbc format parameters...")
            # 尝试指定 sbc 格式
            command_explicit = [
                'ffmpeg', '-y',
                '-f', 'sbc',
                '-i', input_sbc,
                output_wav
            ]
            result_retry = subprocess.run(command_explicit, capture_output=True, text=True)
            if result_retry.returncode == 0:
                 print(f"Successfully created {output_wav} (with explicit format)")
            else:
                 print("ffmpeg explicit retry failed:")
                 print(result_retry.stderr)
    except FileNotFoundError:
        print("Error: ffmpeg not found. Please make sure ffmpeg is installed and in your PATH.")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Convert spaced-hex SBC logs to .wav using ffmpeg."
    )
    parser.add_argument(
        "input_txt",
        nargs="?",
        default="sbc0503.txt",
        help="Input raw SBC stream captured as .txt (default: sbc0503.txt)",
    )
    parser.add_argument(
        "output_wav",
        nargs="?",
        default=None,
        help="Output WAV file (default: same name as input_txt with .wav extension)",
    )
    return parser.parse_args()


def derive_output_wav(input_txt, output_wav):
    if output_wav:
        return output_wav
    base_name, _ = os.path.splitext(input_txt)
    return base_name + ".wav"

if __name__ == "__main__":
    args = parse_args()
    input_txt = args.input_txt
    target_wav = derive_output_wav(input_txt, args.output_wav)

    if os.path.exists(input_txt):
        temp_sbc_path = None
        try:
            with tempfile.NamedTemporaryFile(suffix=".sbc", delete=False) as temp_file:
                temp_sbc_path = temp_file.name

            if convert_txt_hex_to_sbc(input_txt, temp_sbc_path):
                convert_sbc_to_wav(temp_sbc_path, target_wav)
        finally:
            if temp_sbc_path and os.path.exists(temp_sbc_path):
                os.remove(temp_sbc_path)
    else:
        print(f"Input file {input_txt} not found.")
