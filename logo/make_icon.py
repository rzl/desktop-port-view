# -*- coding: utf-8 -*-
"""
生成「肉丸子桌面端口管理工具」图标：黑白色调简笔画，肉丸子 + RJ45 网络端口。
以 256x256 坐标系定义图形，按目标尺寸超采样 4 倍绘制后缩小，保证抗锯齿。
输出：
  logo/icon_256.png / icon_16.png        —— 预览图
  肉丸子桌面端口管理工具.ico              —— 多尺寸主图标
  small.ico                              —— 小图标（16/24/32 简化版）
"""
import io
import os
import struct

from PIL import Image, ImageDraw

SS = 4  # 超采样倍数
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BLACK = (17, 17, 17, 255)
WHITE = (255, 255, 255, 255)


def _canvas(size, scale):
    """按 256 基准坐标乘缩放系数建画布，返回 (img, draw, k, lw)。"""
    S = round(size * SS)
    k = S / 256.0 / scale  # scale: 图形在画布中的占比（>1 表示画得更满）
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    return img, d, k, lambda w: max(1, round(w * k))


def draw_full(size):
    """完整设计：肉丸子 + 纹理弧线 + RJ45 插座 + 触点 + 下垂网线。用于 >=48。"""
    img, d, k, lw = _canvas(size, 1.0)
    cx, cy, r = 128 * k, 136 * k, 90 * k
    # 肉丸子主体
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=WHITE, outline=BLACK, width=lw(14))
    # 表面纹理：顶部一道弧 + 左下一道弧
    for rr, a0, a1 in ((70, 230, 310), (76, 105, 158)):
        d.arc([cx - rr, cy - rr, cx + rr, cy + rr], a0, a1, fill=BLACK, width=lw(8))
    # RJ45 插座外框（略窄，给丸子左右留出呼吸空间）
    d.rounded_rectangle(
        [80 * k, 100 * k, 176 * k, 162 * k], radius=12 * k,
        fill=WHITE, outline=BLACK, width=lw(11),
    )
    # 梯形插孔（上宽下窄）
    d.polygon(
        [(98 * k, 120 * k), (158 * k, 120 * k), (146 * k, 150 * k), (110 * k, 150 * k)],
        fill=WHITE, outline=BLACK, width=lw(8),
    )
    # 两个触点齿
    d.rectangle([117 * k, 120 * k, 127 * k, 138 * k], fill=BLACK)
    d.rectangle([129 * k, 120 * k, 139 * k, 138 * k], fill=BLACK)
    # 网线：从插座底部垂直向下，末端圆头
    d.line([128 * k, 162 * k, 128 * k, 208 * k], fill=BLACK, width=lw(12))
    d.ellipse([120.5 * k, 200.5 * k, 135.5 * k, 215.5 * k], fill=BLACK)
    return img.resize((size, size), Image.LANCZOS)


def draw_mid(size):
    """中等尺寸（24~40）：丸子 + 插座外框 + 梯形孔，去掉纹理和网线。"""
    img, d, k, lw = _canvas(size, 1.0)
    cx, cy, r = 128 * k, 128 * k, 96 * k
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=WHITE, outline=BLACK, width=lw(16))
    d.rounded_rectangle(
        [72 * k, 92 * k, 184 * k, 164 * k], radius=12 * k,
        fill=WHITE, outline=BLACK, width=lw(12),
    )
    d.polygon(
        [(92 * k, 112 * k), (164 * k, 112 * k), (150 * k, 148 * k), (106 * k, 148 * k)],
        fill=WHITE, outline=BLACK, width=lw(9),
    )
    d.rectangle([118 * k, 112 * k, 126 * k, 130 * k], fill=BLACK)
    d.rectangle([130 * k, 112 * k, 138 * k, 130 * k], fill=BLACK)
    return img.resize((size, size), Image.LANCZOS)


def draw_small(size):
    """极简（16/20）：丸子 + 黑色实心梯形孔。"""
    img, d, k, lw = _canvas(size, 1.04)
    cx, cy, r = 128 * k, 128 * k, 102 * k
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=WHITE, outline=BLACK, width=lw(22))
    d.polygon(
        [(88 * k, 106 * k), (168 * k, 106 * k), (152 * k, 152 * k), (104 * k, 152 * k)],
        fill=BLACK,
    )
    return img.resize((size, size), Image.LANCZOS)


def render(size):
    if size >= 48:
        return draw_full(size)
    if size >= 24:
        return draw_mid(size)
    return draw_small(size)


def write_ico(path, sizes):
    """手写 ICO 容器：每个尺寸嵌入一张 PNG（Vista+ 均支持）。"""
    entries, blobs = [], []
    offset = 6 + 16 * len(sizes)
    for s in sizes:
        buf = io.BytesIO()
        render(s).save(buf, "PNG")
        data = buf.getvalue()
        entries.append(struct.pack(
            "<BBBBHHII",
            s if s < 256 else 0, s if s < 256 else 0, 0, 0,
            1, 32, len(data), offset,
        ))
        blobs.append(data)
        offset += len(data)
    with open(path, "wb") as f:
        f.write(struct.pack("<HHH", 0, 1, len(sizes)))
        for e in entries:
            f.write(e)
        for b in blobs:
            f.write(b)


def main():
    # 预览图
    render(256).save(os.path.join(HERE, "icon_256.png"))
    render(16).resize((64, 64), Image.NEAREST).save(os.path.join(HERE, "icon_16_x4.png"))
    # 程序图标
    write_ico(os.path.join(ROOT, "肉丸子桌面端口管理工具.ico"),
              [16, 20, 24, 32, 40, 48, 64, 128, 256])
    write_ico(os.path.join(ROOT, "small.ico"), [16, 24, 32])
    print("done")


if __name__ == "__main__":
    main()
