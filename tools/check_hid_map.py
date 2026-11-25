# -*- coding: utf-8 -*-
"""校验 bleuart/src/ble_hid_kbd.c 里的 HID 报告描述符（离线，无需硬件）。

手写 HID 描述符只要错一个字节，设备就可能完全不被识别为 HID，而且不会有
任何编译期报错。本脚本按 HID 1.11 的 item 规则遍历一遍，并核对每个
Report ID 的输入位宽与 ble_hid_kbd.h 里的长度宏是否一致。

用法：python tools/check_hid_map.py
"""
import re
import sys
import io
from pathlib import Path

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

root = Path(__file__).resolve().parents[1]
SRC = root / "bleuart" / "src" / "ble_hid_kbd.c"
HDR = root / "bleuart" / "inc" / "ble_hid_kbd.h"

src = SRC.read_text(encoding="utf-8-sig")
m = re.search(r"static const uint8_t hid_report_map\[\]\s*=\s*\{(.*?)\n\};", src, re.S)
assert m, "找不到 hid_report_map[]"
body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
body = re.sub(r"//[^\n]*", "", body)
vals = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", body)]
print("描述符共 %d 字节" % len(vals))

# HID item 前缀 = (tag << 4) | (type << 2) | size
MAIN = {0x80: "Input", 0x90: "Output", 0xB0: "Feature",
        0xA0: "Collection", 0xC0: "EndCollection"}
GLOB = {0x00: "UsagePage", 0x10: "LogicalMin", 0x20: "LogicalMax",
        0x30: "PhysicalMin", 0x40: "PhysicalMax", 0x50: "UnitExp",
        0x60: "Unit", 0x70: "ReportSize", 0x80: "ReportID", 0x90: "ReportCount"}
LOCL = {0x00: "Usage", 0x10: "UsageMin", 0x20: "UsageMax"}

i = 0
usage_page = report_id = rpt_size = rpt_count = None
lg_max = us_min = us_max = None
depth = 0
reports = {}
consumer_ok = False
errors = []

while i < len(vals):
    b = vals[i]
    if b == 0xFE:
        errors.append("出现长 item 0xFE @%d" % i)
        break
    size = (0, 1, 2, 4)[b & 0x03]
    typ = (b >> 2) & 0x03
    tag = b & 0xF0
    data = vals[i + 1:i + 1 + size]
    if len(data) != size:
        errors.append("item 截断 @%d" % i)
        break
    val = sum(d << (8 * k) for k, d in enumerate(data))

    if typ == 0:
        name = MAIN.get(tag)
        if name is None:
            errors.append("未知 main item 0x%02X @%d" % (b, i))
        elif name == "Collection":
            depth += 1
        elif name == "EndCollection":
            depth -= 1
            if depth < 0:
                errors.append("EndCollection 多余 @%d" % i)
        elif tag == 0x80:                      # Input
            if rpt_size is None or rpt_count is None:
                errors.append("Input 前缺 ReportSize/ReportCount @%d" % i)
            else:
                reports[report_id] = reports.get(report_id, 0) + rpt_size * rpt_count
                if (usage_page == 0x0C and rpt_size == 16 and rpt_count == 1
                        and data[0] == 0x00
                        and lg_max == 0x03FF and us_min == 0 and us_max == 0x03FF):
                    consumer_ok = True
    elif typ == 1:
        name = GLOB.get(tag)
        if name == "UsagePage":
            usage_page = val
        elif name == "ReportID":
            report_id = val
        elif name == "ReportSize":
            rpt_size = val
        elif name == "ReportCount":
            rpt_count = val
        elif name == "LogicalMax":
            lg_max = val
    else:
        name = LOCL.get(tag)
        if name == "UsageMin":
            us_min = val
        elif name == "UsageMax":
            us_max = val

    i += 1 + size

print("item 遍历结束 @%d/%d，collection 深度 %d" % (i, len(vals), depth))
if depth != 0:
    errors.append("collection 深度未归零：%d" % depth)

print("\n各 Report ID 的输入位宽：")
for rid in sorted(reports, key=lambda x: (x is None, x)):
    bits = reports[rid]
    print("  Report ID %-4s : %3d bit = %d 字节" % (rid, bits, (bits + 7) // 8))

print()
if consumer_ok:
    print("OK  Consumer 报告：16 位 Array，Usage 0x0000~0x03FF，Logical 0~0x03FF")
else:
    errors.append("Consumer 报告不是预期的 16 位 Array 形式")

hdr = HDR.read_text(encoding="utf-8-sig")
kbd_len = int(re.search(r"HID_KBD_REPORT_LEN\s+\((\d+)u\)", hdr).group(1))
con_len = int(re.search(r"HID_CONSUMER_REPORT_LEN\s+\((\d+)u\)", hdr).group(1))
print("\n宏 vs 描述符：")
for rid, want, name in ((1, kbd_len, "HID_KBD_REPORT_LEN"),
                        (2, con_len, "HID_CONSUMER_REPORT_LEN")):
    got = (reports.get(rid, 0) + 7) // 8
    ok = (got == want)
    print("  %-23s = %d，描述符算出 %d  %s" % (name, want, got, "OK" if ok else "不匹配"))
    if not ok:
        errors.append("%s 与描述符不一致（宏 %d / 描述符 %d）" % (name, want, got))

if errors:
    print("\nFAIL:")
    for e in errors:
        print("   -", e)
    raise SystemExit(1)
print("\nOK  描述符结构合法，长度宏一致")
