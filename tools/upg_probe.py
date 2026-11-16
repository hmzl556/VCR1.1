#!/usr/bin/env python3
"""P1 上板联调助手：解析遥控器发来的认证帧并算出要回写的摘要。

用法：
  # 1) 收到 FA 20 00 ... 的 52 字节通知后，把整帧十六进制粘进来
  python tools/upg_probe.py auth "FA200000000102030405060708090a0b0c0d0e0f10111213...60字节"

  # 2) 脚本输出应回写的 F9 20 <hash>

  # 3) 若主机还要发起第二轮，把收到的 F9 21 00 rand.. key_index 粘进来
  python tools/upg_probe.py hash "F92100<16字节rand><key_index>"

  # 4) 生成一条读取请求（rel0 / usr1 / usr2）
  python tools/upg_probe.py read rel0 128

开发占位密钥（未产测写入时固件使用）：
  idcode = b"VCR1DEVIDCODE" + b"0"*19     (32 字节)
  key    = b"VCR1DEVKEY"    + b"0"*6      (16 字节)
"""
import hashlib
import sys

DEV_IDCODE = b"VCR1DEVIDCODE" + b"0" * 19
DEV_KEYS = [b"VCR1DEVKEY" + b"0" * 6]

TYPE_TAGS = {"rel0": b"rel0", "usr1": b"usr1", "usr2": b"usr2"}


def _clean(text):
    return bytes.fromhex("".join(text.split()))


def _hash(key, rand, idcode):
    return hashlib.md5(key + rand + idcode).digest()


def cmd_auth(frame_hex):
    f = _clean(frame_hex)
    if len(f) < 52 or f[0] != 0xFA or f[1] != 0x20 or f[2] != 0x00:
        raise SystemExit("需要一个 FA 20 00 开头的 52 字节认证发起帧")
    key_index = f[3]
    rand = f[4:20]
    idcode = f[20:52]
    key = DEV_KEYS[key_index] if key_index < len(DEV_KEYS) else DEV_KEYS[0]

    digest = _hash(key, rand, idcode)
    print(f"key_index : {key_index}")
    print(f"rand1     : {rand.hex()}")
    print(f"idcode    : {idcode.decode('ascii', 'replace')}")
    if idcode != DEV_IDCODE:
        print("注意       : idcode 与开发占位值不同（可能已产测写入），摘要按帧内实际值计算")
    print()
    print(f"回写(十六进制) : F920{digest.hex()}")
    print(f"预期收到       : FA2001")


def cmd_hash(frame_hex):
    f = _clean(frame_hex)
    if len(f) < 21 or f[0] != 0xF9 or f[1] != 0x21:
        raise SystemExit("需要一个 F9 21 00 <rand16> <key_index> 帧")
    rand = f[3:19]
    key_index = f[19] if len(f) > 19 else 0
    key = DEV_KEYS[key_index] if key_index < len(DEV_KEYS) else DEV_KEYS[0]
    digest = _hash(key, rand, DEV_IDCODE)
    print(f"rand2     : {rand.hex()}")
    print(f"回写(十六进制) : FA21{digest.hex()}")


def cmd_read(type_name, pack_len):
    tag = TYPE_TAGS.get(type_name)
    if tag is None:
        raise SystemExit(f"类型只能是 {', '.join(TYPE_TAGS)}")
    frame = bytes([0xF9, 0x11, int(pack_len)]) + tag + b"\x00" * (8 - len(tag))
    print(f"发送(十六进制) : {frame.hex().upper()}")
    print("预期           : FA 11 00 <addr4> <len4>，随后若干 FA 14 <seq> <len> <data>，"
          "最后 FA 14 00 00")


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    what = argv[1]
    if what == "auth":
        cmd_auth(argv[2])
    elif what == "hash":
        cmd_hash(argv[2])
    elif what == "read":
        cmd_read(argv[2] if len(argv) > 2 else "rel0", argv[3] if len(argv) > 3 else 128)
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
