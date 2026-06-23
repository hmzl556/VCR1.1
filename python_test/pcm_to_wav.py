#!/usr/bin/env python3
import argparse
import os
import wave

# 固定音频参数
SAMPLE_RATE = 16000
CHANNELS = 1
SAMPLE_WIDTH = 2  # bytes, 2 means 16-bit


def parse_args():
    parser = argparse.ArgumentParser(
        description="Convert raw PCM file to WAV file playable by common media players."
    )
    parser.add_argument("input_pcm", help="Input .pcm file path")
    parser.add_argument("output_wav", help="Output .wav file path")
    return parser.parse_args()


def main():
    args = parse_args()

    if not os.path.isfile(args.input_pcm):
        print(f"输入文件不存在: {args.input_pcm}")
        return

    frame_bytes = CHANNELS * SAMPLE_WIDTH

    with open(args.input_pcm, "rb") as f:
        raw = f.read()

    if not raw:
        print("输入 PCM 为空，未生成 WAV。")
        return

    remainder = len(raw) % frame_bytes
    if remainder:
        valid_len = len(raw) - remainder
        print(
            f"警告: PCM 数据长度不是完整帧的整数倍，已丢弃末尾 {remainder} 字节。"
        )
        raw = raw[:valid_len]

    frame_count = len(raw) // frame_bytes
    duration = frame_count / float(SAMPLE_RATE)

    with wave.open(args.output_wav, "wb") as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(SAMPLE_WIDTH)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(raw)

    print(
        "转换完成: "
        f"{args.input_pcm} -> {args.output_wav}, "
        f"采样率={SAMPLE_RATE}Hz, 声道={CHANNELS}, 位宽={SAMPLE_WIDTH * 8}bit, "
        f"时长={duration:.3f}s"
    )


if __name__ == "__main__":
    main()
