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


def crc16(buf, crc=0):
    for b in buf:
        ds = b
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if ((crc ^ ds) & 1) else 0)
            ds >>= 1
    return crc & 0xFFFF


def cmd_write(type_name, pack_len, payload_hex):
    """生成整段写入流程的所有帧（含 CRC16），逐条复制到 nRF Connect 即可。"""
    tag = type_name.encode()
    if len(tag) > 8:
        raise SystemExit("类型最长 8 字节")
    data = _clean(payload_hex) if payload_hex else bytes(range(64))
    pack_len = int(pack_len)
    crc = crc16(data)

    start = (bytes([0xF9, 0x12, pack_len]) + tag + b"\x00" * (8 - len(tag))
             + bytes(4) + len(data).to_bytes(4, "big"))
    print(f"数据长度 {len(data)} 字节，包长 {pack_len}，CRC16 = {crc:04X}\n")
    print("① 发 : F916F5F5F500")
    print("   收 : FA1600")
    print()
    print("② 发 :", start.hex().upper())
    print("   收 : FA1200 + 4字节地址 +", len(data).to_bytes(4, "big").hex().upper())
    print()
    seq = 0
    for off in range(0, len(data), pack_len):
        chunk = data[off:off + pack_len]
        frame = bytes([0xF9, 0x14, seq, len(chunk)]) + chunk
        print(f"③-{seq + 1} 发 :", frame.hex().upper())
        print(f"      收 : FA18{seq:02X}{len(chunk):02X}")
        seq += 1
    print()
    print("   最后一包之后还会再收到 : FA140000（接收完毕）")
    print()
    print(f"④ 发 : F913{crc >> 8:02X}{crc & 0xFF:02X}")
    print(f"   收 : FA1300{crc >> 8:02X}{crc & 0xFF:02X}   ← 两个 CRC 相同时才算提交成功")
    print()
    print("⑤ 发 : F91600000000        （清除升级标志）")
    print("   收 : FA1600")


def build_etv_demo():
    """造一份符合表 E.24 的红外基础信息文件（PW 编码，单协议表，3 个按键）。

    参数取自附录 I 的 PW 示例：38kHz/1:3、unit 560、0=560/560、1=560/1680、
    header 9000/4500、burst 567/40117。按键给 3 个，码值各不相同便于分辨。
    """
    table = bytes([
        0x01,               # enc = PW
        0x02, 0x30,         # unit 560
        0x00,               # has_start
        0x00,               # has_toggle_bit
        0x20,               # data_bit_num 32
        0x00,               # header2_pos
        0x01,               # col_num 1
    ]) + b"".join(v.to_bytes(2, "big") for v in (
        560, 560,           # b0 0/1
        560, 1680,          # b1 0/1
        0, 0,               # b2
        0, 0,               # b3
        9000, 4500,         # header
        0, 0,               # header2
    )) + bytes([
        0x20,               # bit_num 32
        0x00,               # ref_col_index 0
        0x01,               # has_header 1
    ]) + b"".join(v.to_bytes(2, "big") for v in (567, 40117))   # burst

    keys = b""
    for key_id, code in ((3, 0x10), (10, 0x11), (18, 0x12)):
        keys += key_id.to_bytes(2, "big") + bytes([0x00]) + code.to_bytes(8, "little")

    body = table + keys
    hdr = (b"ETV\0" + (1).to_bytes(2, "big") + bytes(6)
           + (1).to_bytes(4, "big") + (28 + len(body)).to_bytes(4, "big")
           + bytes([0x32, 0x01, 0x01, 0x01]) + (3).to_bytes(2, "big") + bytes(2))
    return hdr + body


def cmd_etv(type_name):
    """生成演示 ETV 文件并打印整段写入流程的帧。"""
    blob = build_etv_demo()
    print(f"演示 ETV 文件 {len(blob)} 字节，含按键 key_id=3/10/18"
          f"（按键 3 对应物理 KEY3「电视」，key_id 按物理键号占位）")
    print("码值：key_id=3 → 0x10、10 → 0x11、18 → 0x12（NEC 风格，便于用学习机/示波器分辨）\n")
    cmd_write(type_name, 128, blob.hex())


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
    elif what == "write":
        cmd_write(argv[2] if len(argv) > 2 else "irstb",
                  argv[3] if len(argv) > 3 else 64,
                  argv[4] if len(argv) > 4 else "")
    elif what == "etv":
        cmd_etv(argv[2] if len(argv) > 2 else "irstb")
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
