# -*- coding: utf-8 -*-
"""Vioce_Cat1_Remote_V1.1.pdf 网表连通性分析:
提取所有水平/垂直线段 + 文本标签, 按坐标容差做并查集, 输出关键网的所有标签与器件端点。
"""
import pymupdf, sys

TOL = 3.0  # 坐标容差(pt)

d = pymupdf.open(r'd:\ZZW\桌面\ble\GitHub\VCR1.1\doc\Vioce_Cat1_Remote_V1.1.pdf')
p = d[0]

# ---- 1. 线段: 只取水平/垂直短线(导线), 过滤长斜线/粗框 ----
segs = []
for drw in p.get_drawings():
    for it in drw['items']:
        if it[0] == 'l':
            x0, y0, x1, y1 = it[1].x, it[1].y, it[2].x, it[2].y
            dx, dy = abs(x1-x0), abs(y1-y0)
            if (dx < 0.5 or dy < 0.5) and max(dx, dy) <= 800:  # 正交线, 非大外框
                segs.append((min(x0,x1), min(y0,y1), max(x0,x1), max(y0,y1)))
        elif it[0] == 're':
            x0, y0, x1, y1 = it[1].x0, it[1].y0, it[1].x1, it[1].y1
            w, h = x1-x0, y1-y0
            if (w < 0.5 or h < 0.5) and max(w, h) <= 800:
                segs.append((min(x0,x1), min(y0,y1), max(x0,x1), max(y0,y1)))

# ---- 2. 文本标签: rawdict 里 span.text 为空, 需从 chars 拼 ----
labels = []
for blk in p.get_text('rawdict')['blocks']:
    for line in blk.get('lines', []):
        for span in line['spans']:
            chars = span.get('chars') or []
            t = ''.join(c['c'] for c in chars).strip()
            if t:
                labels.append((t, span['bbox']))

# ---- 3. 并查集: 网格桶加速, 把相互接触(容差)的线段聚成网 ----
parent = list(range(len(segs)))

def find(i):
    while parent[i] != i:
        parent[i] = parent[parent[i]]
        i = parent[i]
    return i

def union(i, j):
    ri, rj = find(i), find(j)
    if ri != rj:
        parent[ri] = rj

CELL = 40.0
grid = {}
for i, (x0, y0, x1, y1) in enumerate(segs):
    for gx in range(int((x0-TOL)//CELL), int((x1+TOL)//CELL)+1):
        for gy in range(int((y0-TOL)//CELL), int((y1+TOL)//CELL)+1):
            grid.setdefault((gx, gy), []).append(i)

def touch(s1, s2):
    x0, y0, x1, y1 = s1
    a0, b0, a1, b1 = s2
    ox = min(x1, a1) - max(x0, a0)
    oy = min(y1, b1) - max(y0, b0)
    return ox >= -TOL and oy >= -TOL

for i, (x0, y0, x1, y1) in enumerate(segs):
    gx, gy = int(x0//CELL), int(y0//CELL)
    for gi in (gx-1, gx, gx+1):
        for gj in (gy-1, gy, gy+1):
            for j in grid.get((gi, gj), ()):
                if j <= i:
                    continue
                if touch(segs[i], segs[j]):
                    union(i, j)

# 网 -> 线段索引集合
nets = {}
for i in range(len(segs)):
    nets.setdefault(find(i), []).append(i)

def net_of_label(t):
    for (txt, bbox) in labels:
        if txt == t:
            cx = (bbox[0]+bbox[2])/2; cy = (bbox[1]+bbox[3])/2
            for rid, idxs in nets.items():
                for i in idxs:
                    x0, y0, x1, y1 = segs[i]
                    if x0-TOL <= cx <= x1+TOL and y0-TOL <= cy <= y1+TOL:
                        return rid, bbox
            return None, bbox
    return None, None

def net_labels(rid):
    out = []
    for (txt, bbox) in labels:
        cx = (bbox[0]+bbox[2])/2; cy = (bbox[1]+bbox[3])/2
        for i in nets.get(rid, []):
            x0, y0, x1, y1 = segs[i]
            if x0-TOL <= cx <= x1+TOL and y0-TOL <= cy <= y1+TOL:
                out.append(txt)
    return out

def net_bbox(rid):
    xs = [segs[i][0] for i in nets[rid]] + [segs[i][2] for i in nets[rid]]
    ys = [segs[i][1] for i in nets[rid]] + [segs[i][3] for i in nets[rid]]
    return (min(xs), min(ys), max(xs), max(ys))

QUERY = ['PB1', 'LTE_RXD', 'LTE_TXD', 'PB5_IRRX', 'PB5', 'VDD_EXT', 'VDD_BAT',
         'PB2', 'LTE_EN', 'VLTE', 'PB4', 'D1', 'B5817WS', 'Q3', 'R30',
         'PB1_LOG_TX', 'PB3', 'PB7', 'PB6', 'PB11', 'PB12', 'PB13']

seen = set()
for q in QUERY:
    rid, bbox = net_of_label(q)
    if rid is None:
        print(f'{q}: (未挂在导线上) bbox={[round(v,1) for v in bbox] if bbox else None}')
        continue
    if rid in seen:
        continue
    seen.add(rid)
    lbls = net_labels(rid)
    nb = net_bbox(rid)
    print(f'=== 网 #{rid} bbox=({nb[0]:.0f},{nb[1]:.0f})-({nb[2]:.0f},{nb[3]:.0f})')
    print('   标签:', ', '.join(sorted(set(lbls))))
