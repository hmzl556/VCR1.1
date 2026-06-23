#!/usr/bin/env python3
import sys
import array
import re

def main():
    if len(sys.argv) < 3:
        print("Usage: python hex_to_pcm.py input_hex.txt output.pcm")
        return

    # 1. 读取文本并匹配所有十六进制 (如 34 12 AB 00)
    with open(sys.argv[1], 'r', encoding='utf-8', errors='ignore') as f:
        raw_text = f.read()
    
    # 删除连续的 00 00 00 00 (匹配 8 个零，可能带空格)
    # 比如 "00 00 00 00" 或 "00000000"
    cleaned_text = re.sub(r'(00\s*){4}', '', raw_text)
    
    # 优先匹配16位十六进制词（如 "1234"），按文本顺序直接解析
    hex_words = re.findall(r'(?<![0-9a-fA-F])[0-9a-fA-F]{4}(?![0-9a-fA-F])', cleaned_text)
    # 兼容字节流格式（如 "12 34 AB CD"），按输入顺序组16位（高字节在前）
    hex_data = re.findall(r'[0-9a-fA-F]{2}', cleaned_text)

    # 2. 解析16位整数（不做小端拼接）
    # 3. 保持原始有符号数据，不做中点偏置处理
    pcm_samples = array.array('h')
    if hex_words:
        for word in hex_words:
            adc = int(word, 16) & 0xFFFF
            if adc >= 0x8000:
                adc -= 0x10000
            pcm_samples.append(adc)
    else:
        for i in range(0, len(hex_data) - 1, 2):
            # 按输入顺序组16位（小端）：第1个字节为低8位，第2个字节为高8位
            adc = (int(hex_data[i], 16) | (int(hex_data[i + 1], 16) << 8)) & 0xFFFF
            if adc >= 0x8000:
                adc -= 0x10000
            pcm_samples.append(adc)

    # 4. 写入二进制文件 (s16le)
    with open(sys.argv[2], 'wb') as f:
        pcm_samples.tofile(f)

    print(f"转换完成: {len(pcm_samples)} 个采样点 -> {sys.argv[2]}")

if __name__ == "__main__":
    main()
