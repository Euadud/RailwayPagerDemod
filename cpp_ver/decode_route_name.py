#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
POCSAG route message decoder
Decodes fields such as vehicle ID, coordinates, and GB2312 route name.
"""

import re
import itertools
import binascii

# === 映射表 (来自 numeric message 映射规则) ===
numeric_chars = "0123456789.U -)("

# nibble reverse lookup
def reverse4(x):
    return int('{:04b}'.format(x)[::-1], 2)

def str_to_nibbles(s):
    """把 POCSAG 数字段解成 nibble 序列"""
    table = {c: i for i, c in enumerate(numeric_chars)}
    nibbles = []
    for ch in s:
        if ch in table:
            nibbles.append(table[ch])
    return nibbles

def decode_gb2312_from_numeric(numeric_field):
    """尝试多种组合方式来恢复 GB2312 字节并解码"""
    nibbles = str_to_nibbles(numeric_field)
    results = []
    for reverse_bits, high_first in itertools.product([False, True], [False, True]):
        b = bytearray()
        for i in range(0, len(nibbles)-1, 2):
            n1, n2 = nibbles[i], nibbles[i+1]
            if reverse_bits:
                n1, n2 = reverse4(n1), reverse4(n2)
            if high_first:
                byte = (n1 << 4) | n2
            else:
                byte = (n2 << 4) | n1
            b.append(byte)
        try:
            text = b.decode("gb2312", errors="ignore")
            if any('\u4e00' <= c <= '\u9fff' for c in text):
                results.append(text)
        except Exception:
            pass
    return list(set(results))  # 去重

def decode_message(msg):
    # 示例格式：20202480021530U).9UU.6 (-(202011625775639540394000
    # 规则：前 10 位是车号 + 时间，最后一段是经纬度，中间是线路名
    m = re.match(r"(\d{10})([U\.\d\-\(\) ]+)(\d{6})(\d{9})(\d{3})", msg)
    if not m:
        print("❌ 格式无法匹配:", msg)
        return
    vehicle_id = m.group(1)
    route_raw = m.group(2).strip()
    lat_raw = m.group(3)
    lon_raw = m.group(4)

    # 经纬度转为浮点
    latitude = float(lat_raw[:2] + "." + lat_raw[2:])
    longitude = float(lon_raw[:3] + "." + lon_raw[3:])

    # 线路名解码
    candidates = decode_gb2312_from_numeric(route_raw)
    route_name = candidates[0] if candidates else "(无法解码)"

    print("🚗 Vehicle ID:", vehicle_id)
    print("🧭 Latitude :", latitude)
    print("🧭 Longitude:", longitude)
    print("📍 Route Raw:", route_raw)
    print("📍 Route Name:", route_name)
    if len(candidates) > 1:
        print("📜 其他候选：", candidates[1:])

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python decode_route_name.py <raw_message>")
        sys.exit(1)
    decode_message(sys.argv[1])

