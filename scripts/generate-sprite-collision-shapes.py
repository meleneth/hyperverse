#!/usr/bin/env python3
"""Generate sprite alpha collision hull data.

The output is checked in so runtime collision does not need to decode PNG files.
Each sprite is split into horizontal alpha bands and each band becomes a convex
hull part. Jolt then uses a compound shape, which preserves large transparent
cutouts better than a single sprite-wide hull.
"""

from __future__ import annotations

import argparse
import pathlib
import struct
import zlib


SPRITES = {
    "Ship": "ship.png",
    "Rock": "rock1.png",
    "Particle": "particle.png",
}


def read_png_rgba(path: pathlib.Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG: {path}")

    offset = 8
    width = 0
    height = 0
    color_type = 0
    bit_depth = 0
    payload = bytearray()
    while offset < len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        chunk = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, _, _, _ = struct.unpack(">IIBBBBB", chunk)
        elif chunk_type == b"IDAT":
            payload.extend(chunk)
        elif chunk_type == b"IEND":
            break

    if bit_depth != 8 or color_type != 6:
        raise ValueError(f"expected 8-bit RGBA PNG: {path}")

    raw = zlib.decompress(bytes(payload))
    stride = width * 4
    rows: list[bytearray] = []
    cursor = 0
    for _ in range(height):
        filter_type = raw[cursor]
        cursor += 1
        row = bytearray(raw[cursor : cursor + stride])
        cursor += stride
        prior = rows[-1] if rows else bytearray(stride)
        for i in range(stride):
            left = row[i - 4] if i >= 4 else 0
            up = prior[i]
            up_left = prior[i - 4] if i >= 4 else 0
            if filter_type == 1:
                row[i] = (row[i] + left) & 0xFF
            elif filter_type == 2:
                row[i] = (row[i] + up) & 0xFF
            elif filter_type == 3:
                row[i] = (row[i] + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                predictor = paeth(left, up, up_left)
                row[i] = (row[i] + predictor) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"unsupported PNG filter {filter_type}: {path}")
        rows.append(row)
    return width, height, b"".join(rows)


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def solid(rgba: bytes, width: int, x: int, y: int, threshold: int) -> bool:
    return rgba[((y * width + x) * 4) + 3] >= threshold


def normalized(width: int, height: int, x: int, y: int) -> tuple[float, float]:
    scale = 2.0 / max(width, height)
    return ((x + 0.5 - (width * 0.5)) * scale, ((height * 0.5) - (y + 0.5)) * scale)


def cross(origin: tuple[float, float], a: tuple[float, float], b: tuple[float, float]) -> float:
    return (a[0] - origin[0]) * (b[1] - origin[1]) - (a[1] - origin[1]) * (b[0] - origin[0])


def hull(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    points = sorted(set(points))
    if len(points) <= 2:
        return points
    lower: list[tuple[float, float]] = []
    for point in points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], point) <= 0.0:
            lower.pop()
        lower.append(point)
    upper: list[tuple[float, float]] = []
    for point in reversed(points):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], point) <= 0.0:
            upper.pop()
        upper.append(point)
    return lower[:-1] + upper[:-1]


def sprite_parts(path: pathlib.Path, bands: int, threshold: int) -> list[list[tuple[float, float]]]:
    width, height, rgba = read_png_rgba(path)
    parts: list[list[tuple[float, float]]] = []
    for band in range(bands):
        y0 = (height * band) // bands
        y1 = (height * (band + 1)) // bands
        points: list[tuple[float, float]] = []
        for y in range(y0, y1):
            for x in range(width):
                if not solid(rgba, width, x, y, threshold):
                    continue
                boundary = x == 0 or y == 0 or x + 1 == width or y + 1 == height
                boundary = boundary or not solid(rgba, width, x - 1, y, threshold)
                boundary = boundary or not solid(rgba, width, x + 1, y, threshold)
                boundary = boundary or not solid(rgba, width, x, y - 1, threshold)
                boundary = boundary or not solid(rgba, width, x, y + 1, threshold)
                if boundary:
                    points.append(normalized(width, height, x, y))
        band_hull = hull(points)
        if len(band_hull) >= 3:
            parts.append(band_hull)
    return parts


def emit(source_dir: pathlib.Path, output: pathlib.Path, bands: int, threshold: int) -> None:
    lines = [
        '#include "hyperverse/sprite_collision_shape_data.hpp"',
        "",
        "#include <array>",
        "",
        "namespace hyperverse {",
        "namespace {",
        "",
    ]
    part_names: dict[str, list[str]] = {}
    for shape, filename in SPRITES.items():
        part_names[shape] = []
        for index, points in enumerate(sprite_parts(source_dir / filename, bands, threshold)):
            point_name = f"{shape.lower()}_part_{index}_points"
            part_names[shape].append(point_name)
            lines.append(f"constexpr std::array<Vec2, {len(points)}> {point_name}{{{{")
            for x, y in points:
                lines.append(f"  Vec2{{.x = {x:.6f}F, .y = {y:.6f}F}},")
            lines.append("}};")
            lines.append("")
        lines.append(f"constexpr std::array<SpriteCollisionPartView, {len(part_names[shape])}> {shape.lower()}_parts{{{{")
        for point_name in part_names[shape]:
            lines.append(f"  SpriteCollisionPartView{{.points = {point_name}.data(), .point_count = {point_name}.size()}},")
        lines.append("}};")
        lines.append("")

    lines += [
        "}  // namespace",
        "",
        "SpriteCollisionShapeView sprite_collision_shape_data(SpriteCollisionShape shape) {",
        "  switch (shape) {",
    ]
    for shape in SPRITES:
        lines.append(f"    case SpriteCollisionShape::{shape}:")
        lines.append(f"      return {{.parts = {shape.lower()}_parts.data(), .part_count = {shape.lower()}_parts.size()}};")
    lines += [
        "  }",
        "  return {};",
        "}",
        "",
        "}  // namespace hyperverse",
    ]
    output.write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sprites", type=pathlib.Path, default=pathlib.Path("assets/sector7/sprites"))
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("src/sprite_collision_shape_data.cpp"))
    parser.add_argument("--bands", type=int, default=8)
    parser.add_argument("--alpha", type=int, default=16)
    args = parser.parse_args()
    emit(args.sprites, args.output, args.bands, args.alpha)


if __name__ == "__main__":
    main()
