#!/usr/bin/env python3
# This file is part of ProGearSDK.
# Copyright (c) 2024-2025 ProGearSDK contributors
# SPDX-License-Identifier: MIT

"""
ngres - NeoGeo Resource Compiler

Processes visual and audio asset definitions from YAML and generates:
  - C-ROM data (sprite graphics in NeoGeo format)
  - V-ROM data (ADPCM audio samples)
  - C header with asset definitions for the SDK

Usage:
    ngres assets.yaml -o output_dir

YAML format:
    # Optional: explicit palette definitions for sharing or manual colors
    palettes:
      shared_sprites:
        source: player.png       # extract from image
      custom_ui:
        colors: [0x8000, 0xFFFF, ...]  # manual 16-color array

    visual_assets:
      # Static image (palette auto-generated from image)
      - name: sky
        source: assets/sky.png

      # Animated sprite with shared palette
      - name: ball
        source: assets/ball.png
        frame_size: [16, 16]
        palette: shared_sprites   # reference to palette asset
        animations:
          spin: { frames: [0-15], speed: 1, loop: true }

    # Sound effects (ADPCM-A: 18.5kHz, 6 channels, short samples)
    sound_effects:
      - name: jump
        source: assets/audio/jump.wav
      - name: hit
        source: assets/audio/hit.wav

    # Music (ADPCM-B: variable rate, 1 channel, looping)
    music:
      - name: theme
        source: assets/audio/theme.wav
        sample_rate: 22050   # optional, default 22050
"""

import argparse
import os
import sys
import re
import struct
import wave
import xml.etree.ElementTree as ET
from pathlib import Path

try:
    import yaml
except ImportError:
    print("Error: PyYAML required. Install with: pip install pyyaml", file=sys.stderr)
    sys.exit(1)

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow required. Install with: pip install pillow", file=sys.stderr)
    sys.exit(1)


class NGResError(Exception):
    """Custom exception for ngres errors."""
    pass


# ============================================================================
# ADPCM Codecs for YM2610
# Based on Yamaha's ADPCM format used in the YM2610 sound chip
# ============================================================================

class ADPCM_A:
    """
    YM2610 ADPCM-A encoder for sound effects.
    Fixed 18.5kHz sample rate, 12-bit samples encoded to 4-bit.
    """
    # Adaptive step sizes for quantization (49 entries)
    STEP_SIZE = [
        16,   17,   19,   21,   23,   25,   28,
        31,   34,   37,   41,   45,   50,   55,
        60,   66,   73,   80,   88,   97,  107,
        118, 130,  143,  157,  173,  190,  209,
        230, 253,  279,  307,  337,  371,  408,
        449, 494,  544,  598,  658,  724,  796,
        876, 963, 1060, 1166, 1282, 1411, 1552
    ]

    # Step adjustment based on magnitude
    STEP_ADJ = [-1, -1, -1, -1, 2, 5, 7, 9]

    def __init__(self):
        self.reset()

    def reset(self):
        self.step_index = 0
        self.sample12 = 0

    def _encode_sample(self, sample12):
        """Encode a 12-bit sample to 4-bit ADPCM."""
        diff = sample12 - self.sample12
        sign = 0b1000 if diff < 0 else 0
        diff = abs(diff)

        magnitude = 0
        threshold = self.STEP_SIZE[self.step_index]

        if diff >= threshold:
            magnitude |= 0b0100
            diff -= threshold
        threshold >>= 1
        if diff >= threshold:
            magnitude |= 0b0010
            diff -= threshold
        threshold >>= 1
        if diff >= threshold:
            magnitude |= 0b0001

        adpcm4 = sign | magnitude

        # Update state by decoding
        self._decode_sample(adpcm4)
        return adpcm4

    def _decode_sample(self, adpcm4):
        """Decode 4-bit ADPCM to update state."""
        step_size = self.STEP_SIZE[self.step_index]
        sign = adpcm4 & 8
        magnitude = adpcm4 & 7

        quantized_diff = ((2 * magnitude + 1) * step_size) >> 3
        if sign:
            quantized_diff = -quantized_diff

        decoded = self.sample12 + quantized_diff
        decoded = max(-2048, min(decoded, 2047))

        new_index = self.step_index + self.STEP_ADJ[magnitude]
        new_index = max(0, min(new_index, 48))

        self.sample12 = decoded
        self.step_index = new_index
        return decoded

    def encode_s16(self, samples):
        """Encode 16-bit signed samples to ADPCM-A."""
        # Convert to 12-bit
        samples12 = [s >> 4 for s in samples]
        return self.encode(samples12)

    def encode(self, samples12):
        """Encode 12-bit samples to ADPCM-A."""
        self.reset()
        # Pad to 512-sample boundary (256 bytes)
        pad_len = ((len(samples12) + 511) // 512) * 512
        padding = [0] * (pad_len - len(samples12))
        adpcms = [self._encode_sample(s) for s in samples12 + padding]
        return adpcms


class ADPCM_B:
    """
    YM2610 ADPCM-B encoder for music.
    Variable sample rate (1.85kHz - 55.5kHz), 16-bit samples.
    """
    # Step adjustment table
    STEP_TABLE = [57, 57, 57, 57, 77, 102, 128, 153]

    def __init__(self):
        self.reset()

    def reset(self):
        self.step_size = 127
        self.sample16 = 0

    def _encode_sample(self, sample16):
        """Encode a 16-bit sample to 4-bit ADPCM."""
        diff = sample16 - self.sample16
        magnitude = (abs(diff) << 16) // (self.step_size << 14)
        magnitude = min(magnitude, 7)
        sign = 0b1000 if diff < 0 else 0

        adpcm4 = sign | magnitude
        self._decode_sample(adpcm4)
        return adpcm4

    def _decode_sample(self, adpcm4):
        """Decode 4-bit ADPCM to update state."""
        sign = adpcm4 & 8
        magnitude = adpcm4 & 7

        quantized_diff = ((2 * magnitude + 1) * self.step_size) >> 3
        if sign:
            quantized_diff = -quantized_diff

        decoded = self.sample16 + quantized_diff
        decoded = max(-32768, min(decoded, 32767))

        new_step = (self.step_size * self.STEP_TABLE[magnitude]) >> 6
        new_step = max(127, min(new_step, 24576))

        self.sample16 = decoded
        self.step_size = new_step
        return decoded

    def encode_s16(self, samples):
        """Encode 16-bit signed samples to ADPCM-B."""
        return self.encode(samples)

    def encode(self, samples16):
        """Encode 16-bit samples to ADPCM-B."""
        self.reset()
        # Pad to 512-sample boundary (256 bytes)
        pad_len = ((len(samples16) + 511) // 512) * 512
        padding = [0] * (pad_len - len(samples16))
        adpcms = [self._encode_sample(s) for s in list(samples16) + padding]
        return adpcms


def load_wav_file(source_path, yaml_dir, target_rate=None):
    """
    Load a WAV file and return samples and sample rate.
    Handles mono conversion and resampling if needed.
    """
    if not os.path.isabs(source_path):
        source_path = os.path.join(yaml_dir, source_path)

    if not os.path.exists(source_path):
        raise NGResError(f"Audio file not found: {source_path}")

    try:
        with wave.open(source_path, 'rb') as w:
            channels = w.getnchannels()
            sample_width = w.getsampwidth()
            sample_rate = w.getframerate()
            n_frames = w.getnframes()
            raw_data = w.readframes(n_frames)
    except Exception as e:
        raise NGResError(f"Failed to load audio file {source_path}: {e}")

    # Convert to samples (all normalized to 16-bit signed)
    if sample_width == 1:
        # 8-bit unsigned
        samples = struct.unpack(f'{len(raw_data)}B', raw_data)
        samples = [(s - 128) << 8 for s in samples]  # Convert to 16-bit signed
    elif sample_width == 2:
        # 16-bit signed
        samples = struct.unpack(f'<{len(raw_data)//2}h', raw_data)
        samples = list(samples)
    elif sample_width == 3:
        # 24-bit signed - extract high 16 bits
        if len(raw_data) % 3 != 0:
            raise NGResError(f"Invalid 24-bit WAV: data length {len(raw_data)} "
                           f"is not divisible by 3")
        n_samples = len(raw_data) // 3
        samples = []
        for i in range(n_samples):
            # Little-endian 24-bit: low, mid, high bytes
            low = raw_data[i * 3]
            mid = raw_data[i * 3 + 1]
            high = raw_data[i * 3 + 2]
            # Combine to 24-bit signed, then take upper 16 bits
            val = low | (mid << 8) | (high << 16)
            if val >= 0x800000:  # Sign extend
                val -= 0x1000000
            samples.append(val >> 8)  # Keep upper 16 bits
    elif sample_width == 4:
        # 32-bit signed - extract high 16 bits
        samples = struct.unpack(f'<{len(raw_data)//4}i', raw_data)
        samples = [s >> 16 for s in samples]
    else:
        raise NGResError(f"Unsupported sample width: {sample_width} bytes")

    # Convert stereo to mono
    if channels == 2:
        mono_samples = []
        for i in range(0, len(samples), 2):
            mono_samples.append((samples[i] + samples[i + 1]) // 2)
        samples = mono_samples

    # Resample if needed
    if target_rate and sample_rate != target_rate:
        samples = resample_audio(samples, sample_rate, target_rate)
        sample_rate = target_rate

    return samples, sample_rate, source_path


def resample_audio(samples, src_rate, dst_rate):
    """Simple linear interpolation resampling."""
    if src_rate == dst_rate:
        return samples

    ratio = src_rate / dst_rate
    out_len = int(len(samples) / ratio)
    result = []

    for i in range(out_len):
        src_pos = i * ratio
        src_idx = int(src_pos)
        frac = src_pos - src_idx

        if src_idx + 1 < len(samples):
            sample = int(samples[src_idx] * (1 - frac) + samples[src_idx + 1] * frac)
        else:
            sample = samples[src_idx] if src_idx < len(samples) else 0

        result.append(max(-32768, min(32767, sample)))

    return result


def pack_adpcm(adpcm_nibbles):
    """Pack 4-bit ADPCM nibbles into bytes (2 nibbles per byte)."""
    packed = []
    for i in range(0, len(adpcm_nibbles), 2):
        byte = (adpcm_nibbles[i] << 4) | adpcm_nibbles[i + 1]
        packed.append(byte)
    return bytes(packed)


def calculate_delta_n(sample_rate):
    """
    Calculate Delta-N value for ADPCM-B playback rate.
    Delta-N = (sample_rate / 55555) * 65536
    """
    delta_n = int((sample_rate / 55555.0) * 65536)
    return min(delta_n, 0xFFFF)


def parse_frame_spec(spec):
    """
    Parse a frame specification into a list of frame indices.

    Formats:
        12          -> [12]
        [0, 1, 2]   -> [0, 1, 2]
        [0-3]       -> [0, 1, 2, 3]
        [0-3, 8-11] -> [0, 1, 2, 3, 8, 9, 10, 11]
    """
    if isinstance(spec, int):
        return [spec]

    if isinstance(spec, list):
        result = []
        for item in spec:
            if isinstance(item, int):
                result.append(item)
            elif isinstance(item, str):
                # Parse range like "0-3"
                match = re.match(r'^(\d+)-(\d+)$', item.strip())
                if match:
                    start, end = int(match.group(1)), int(match.group(2))
                    if start > end:
                        raise NGResError(f"Invalid frame range '{item}': "
                                       f"start ({start}) > end ({end})")
                    result.extend(range(start, end + 1))
                else:
                    result.append(int(item))
        return result

    if isinstance(spec, str):
        # Single range or number
        match = re.match(r'^(\d+)-(\d+)$', spec.strip())
        if match:
            start, end = int(match.group(1)), int(match.group(2))
            if start > end:
                raise NGResError(f"Invalid frame range '{spec}': "
                               f"start ({start}) > end ({end})")
            return list(range(start, end + 1))
        return [int(spec)]

    raise NGResError(f"Invalid frame specification: {spec}")


def load_and_validate_image(source_path, yaml_dir):
    """Load image (keeps original format to detect animated GIFs)."""
    # Resolve path relative to YAML file
    if not os.path.isabs(source_path):
        source_path = os.path.join(yaml_dir, source_path)

    if not os.path.exists(source_path):
        raise NGResError(f"Source file not found: {source_path}")

    try:
        img = Image.open(source_path)
    except Exception as e:
        raise NGResError(f"Failed to load image {source_path}: {e}")

    return img, source_path


def convert_to_rgba(img):
    """
    Convert image to RGBA, properly handling GIF transparency.
    GIF files use palette-based transparency which PIL doesn't automatically
    convert to alpha channel.
    """
    if img.mode == 'RGBA':
        return img

    if img.mode == 'P':
        # Check for transparency info in palette mode
        if 'transparency' in img.info:
            # Convert to RGBA, which will use the transparency info
            return img.convert('RGBA')
        elif 'background' in img.info:
            # GIF with background index - only treat as transparent if the
            # background color is black (common convention for transparency)
            bg_idx = img.info['background']
            pal = img.getpalette()
            bg_r, bg_g, bg_b = pal[bg_idx*3:(bg_idx+1)*3]

            # Only treat as transparent if background is black
            if bg_r == 0 and bg_g == 0 and bg_b == 0:
                indexed = list(img.getdata())
                rgba = img.convert('RGBA')
                pixels = list(rgba.getdata())
                new_pixels = []
                for i, (r, g, b, a) in enumerate(pixels):
                    if indexed[i] == bg_idx:
                        new_pixels.append((r, g, b, 0))  # Make transparent
                    else:
                        new_pixels.append((r, g, b, a))
                rgba.putdata(new_pixels)
                return rgba
            else:
                # Background is not black, just convert normally
                return img.convert('RGBA')
        else:
            # No transparency info - just convert
            return img.convert('RGBA')

    # For other modes, just convert
    return img.convert('RGBA')


def extract_frames(img, frame_width, frame_height, source_path):
    """Extract animation frames from sprite sheet or animated GIF."""
    img_width, img_height = img.size

    # Check if this is an animated GIF
    n_frames = getattr(img, 'n_frames', 1)
    is_animated = n_frames > 1

    if is_animated:
        # Animated GIF: extract each animation frame
        if img_width != frame_width or img_height != frame_height:
            raise NGResError(
                f"{source_path}: Animated GIF size {img_width}x{img_height} "
                f"must match frame_size {frame_width}x{frame_height}"
            )

        frames = []
        for i in range(n_frames):
            img.seek(i)
            frame = img.convert('RGBA')
            frames.append(frame.copy())

        return frames, n_frames
    else:
        # Static image: treat as sprite sheet if frame_size differs from image size
        if img.mode != 'RGBA':
            img = convert_to_rgba(img)

        # If frame size matches image size, it's a single frame
        if frame_width == img_width and frame_height == img_height:
            return [img], 1

        # Validate dimensions for sprite sheet
        if img_width % frame_width != 0:
            raise NGResError(
                f"{source_path}: Image width {img_width} is not divisible by frame width {frame_width}"
            )
        if img_height % frame_height != 0:
            raise NGResError(
                f"{source_path}: Image height {img_height} is not divisible by frame height {frame_height}"
            )

        cols = img_width // frame_width
        rows = img_height // frame_height
        frame_count = cols * rows

        frames = []
        for row in range(rows):
            for col in range(cols):
                x = col * frame_width
                y = row * frame_height
                frame = img.crop((x, y, x + frame_width, y + frame_height))
                frames.append(frame)

        return frames, frame_count


def build_palette_from_frames(frames, max_colors=16, asset_name=None):
    """
    Build a single palette from all frames.
    Color 0 is always transparent.
    Returns: palette as list of (r5, g5, b5) tuples
    """
    colors = {}
    for frame in frames:
        pixels = list(frame.getdata())
        for r, g, b, a in pixels:
            if a < 128:
                continue  # Transparent
            # Quantize to 5-bit per channel (NeoGeo native)
            r5 = r >> 3
            g5 = g >> 3
            b5 = b >> 3
            key = (r5, g5, b5)
            colors[key] = colors.get(key, 0) + 1

    # Sort by frequency, take top colors (leave room for transparent)
    sorted_colors = sorted(colors.items(), key=lambda x: -x[1])
    palette = [(-1, -1, -1)]  # Index 0 = transparent (sentinel won't match real colors)
    for color, _ in sorted_colors[:max_colors - 1]:
        palette.append(color)

    if len(sorted_colors) > max_colors - 1:
        name_str = f"'{asset_name}'" if asset_name else "asset"
        print(f"Warning: {name_str} has {len(sorted_colors)} colors but NeoGeo "
              f"only supports {max_colors - 1} (plus transparent). "
              f"Colors will be approximated.", file=sys.stderr)

    return palette


def index_frame_with_palette(frame, palette):
    """Convert frame pixels to palette indices using an existing palette."""
    pixels = list(frame.getdata())
    indexed = []

    # Build a lookup dict for O(1) palette matching instead of O(n) list search
    palette_lookup = {color: idx for idx, color in enumerate(palette)}

    for r, g, b, a in pixels:
        if a < 128:
            indexed.append(0)  # Transparent
        else:
            r5 = r >> 3
            g5 = g >> 3
            b5 = b >> 3
            key = (r5, g5, b5)

            idx = palette_lookup.get(key)
            if idx is None:
                # Find closest color (fallback for colors not in palette)
                idx = 1
                min_dist = float('inf')
                for i, (pr, pg, pb) in enumerate(palette[1:], 1):
                    dist = (r5-pr)**2 + (g5-pg)**2 + (b5-pb)**2
                    if dist < min_dist:
                        min_dist = dist
                        idx = i
            indexed.append(idx)

    return indexed


def tile_to_crom(tile):
    """
    Convert a 16x16 tile to NeoGeo C-ROM format.
    Returns (c1_data, c2_data) - each 64 bytes.
    """
    c1_data = bytearray(64)
    c2_data = bytearray(64)

    # Block coordinates: (start_x, start_y)
    blocks = [
        (0, 0),   # Block 1: top-left
        (0, 8),   # Block 2: bottom-left
        (8, 0),   # Block 3: top-right
        (8, 8),   # Block 4: bottom-right
    ]

    byte_idx = 0
    for block_x, block_y in blocks:
        for row in range(8):
            # Collect 8 pixels for this row (reversed - MSB is leftmost)
            pixels = []
            for col in range(8):
                x = block_x + (7 - col)
                y = block_y + row
                pixels.append(tile[y][x])

            # Extract bitplanes
            bp0 = 0
            bp1 = 0
            bp2 = 0
            bp3 = 0
            for i, px in enumerate(pixels):
                bp0 |= ((px >> 0) & 1) << i
                bp1 |= ((px >> 1) & 1) << i
                bp2 |= ((px >> 2) & 1) << i
                bp3 |= ((px >> 3) & 1) << i

            c1_data[byte_idx] = bp0
            c1_data[byte_idx + 1] = bp1
            c2_data[byte_idx] = bp2
            c2_data[byte_idx + 1] = bp3
            byte_idx += 2

    return bytes(c1_data), bytes(c2_data)


def rgb5_to_neogeo_color(r, g, b):
    """Convert 5-bit RGB to NeoGeo 16-bit color format."""
    r_lsb = r & 1
    g_lsb = g & 1
    b_lsb = b & 1
    r_upper = (r >> 1) & 0xF
    g_upper = (g >> 1) & 0xF
    b_upper = (b >> 1) & 0xF

    color = (r_lsb << 14) | (g_lsb << 13) | (b_lsb << 12)
    color |= (r_upper << 8) | (g_upper << 4) | b_upper

    return color


def process_palette_def(pal_name, pal_def, yaml_dir):
    """
    Process an explicit palette definition from the palettes: section.
    Returns: list of (r5, g5, b5) tuples (16 colors)
    """
    if 'colors' in pal_def:
        # Manual color definition - already in NeoGeo format
        colors = pal_def['colors']
        if len(colors) > 16:
            print(f"Warning: palette '{pal_name}' has more than 16 colors, truncating",
                  file=sys.stderr)
            colors = colors[:16]
        # Convert NeoGeo colors back to RGB5 for consistency
        palette = []
        for i, c in enumerate(colors):
            if i == 0:
                # Use sentinel value that won't match any real color
                # This prevents black (0,0,0) from matching transparent
                palette.append((-1, -1, -1))
            else:
                # Extract RGB5 from NeoGeo format
                r = ((c >> 8) & 0xF) << 1 | ((c >> 14) & 1)
                g = ((c >> 4) & 0xF) << 1 | ((c >> 13) & 1)
                b = (c & 0xF) << 1 | ((c >> 12) & 1)
                palette.append((r, g, b))
        # Pad to 16
        while len(palette) < 16:
            palette.append((0, 0, 0))
        return palette

    elif 'source' in pal_def:
        # Extract palette from image
        source_path = pal_def['source']
        if not os.path.isabs(source_path):
            source_path = os.path.join(yaml_dir, source_path)

        if not os.path.exists(source_path):
            raise NGResError(f"Palette '{pal_name}' source not found: {source_path}")

        try:
            img = Image.open(source_path)
            if img.mode != 'RGBA':
                img = convert_to_rgba(img)
        except Exception as e:
            raise NGResError(f"Failed to load palette source '{source_path}': {e}")

        # Build palette from image
        return build_palette_from_frames([img], max_colors=16, asset_name=pal_name)

    else:
        raise NGResError(
            f"Palette '{pal_name}' must have either 'colors' or 'source'"
        )


def process_visual_asset(asset_def, yaml_dir, base_tile, palette_registry):
    """
    Process a visual asset definition.
    Returns: (tiles_c1, tiles_c2, palette, asset_info, tile_count)

    palette_registry: dict of {name: {'index': int, 'colors': [(r,g,b)...]}}
    """
    name = asset_def.get('name')
    if not name:
        raise NGResError("Visual asset missing 'name' field")

    source = asset_def.get('source')
    if not source:
        raise NGResError(f"Visual asset '{name}' missing 'source' field")

    # Palette can be:
    # - omitted: auto-generate from image, auto-assign index
    # - string: reference to explicit palette in registry
    # - int: legacy support for manual index (will auto-generate palette data)
    palette_ref = asset_def.get('palette')

    # Load image
    img, source_path = load_and_validate_image(source, yaml_dir)
    img_width, img_height = img.size

    # Validate max height
    if img_height > 512:
        raise NGResError(
            f"Visual asset '{name}': Height {img_height} exceeds maximum of 512 pixels"
        )

    # Determine if this is animated
    frame_size = asset_def.get('frame_size')
    if frame_size:
        # Animated asset with frame_size
        if len(frame_size) != 2:
            raise NGResError(f"Visual asset '{name}': frame_size must be [width, height]")
        frame_width, frame_height = frame_size
        if frame_width % 16 != 0 or frame_height % 16 != 0:
            raise NGResError(
                f"Visual asset '{name}': frame_size must be multiples of 16 (got {frame_width}x{frame_height})"
            )
    else:
        # Static image - whole image is one frame
        # Round up to nearest 16 for tiles
        frame_width = img_width
        frame_height = img_height
        if frame_width % 16 != 0:
            raise NGResError(
                f"Visual asset '{name}': Image width {frame_width} must be multiple of 16"
            )
        if frame_height % 16 != 0:
            raise NGResError(
                f"Visual asset '{name}': Image height {frame_height} must be multiple of 16"
            )

    # Extract frames
    frames, frame_count = extract_frames(img, frame_width, frame_height, source_path)

    # Process animations
    animations = asset_def.get('animations', {})
    anim_defs = []

    for anim_name, anim_spec in animations.items():
        if isinstance(anim_spec, dict):
            if 'frame' in anim_spec:
                frame_list = [anim_spec['frame']]
            elif 'frames' in anim_spec:
                frame_list = parse_frame_spec(anim_spec['frames'])
            else:
                raise NGResError(
                    f"Visual asset '{name}' animation '{anim_name}': missing 'frame' or 'frames'"
                )
            speed = anim_spec.get('speed', 4)
            loop = anim_spec.get('loop', True)
        else:
            frame_list = [anim_spec]
            speed = 4
            loop = True

        # Validate frame indices
        for f in frame_list:
            if f < 0 or f >= frame_count:
                raise NGResError(
                    f"Visual asset '{name}' animation '{anim_name}': "
                    f"frame {f} out of range (0-{frame_count-1})"
                )

        anim_defs.append({
            'name': anim_name,
            'first_frame': frame_list[0],
            'frame_count': len(frame_list),
            'speed': speed,
            'loop': 1 if loop else 0,
        })

    # Determine palette to use
    palette_name = None  # Will be set if using a named palette

    if palette_ref is None:
        # Auto-generate palette from image, register with asset name
        palette = build_palette_from_frames(frames, max_colors=16, asset_name=name)
        palette_name = name  # Use asset name as palette name
        if palette_name not in palette_registry:
            palette_registry[palette_name] = {
                'index': palette_registry['_next_index'],
                'colors': palette,
            }
            palette_registry['_next_index'] += 1
        palette_idx = palette_registry[palette_name]['index']

    elif isinstance(palette_ref, str):
        # Reference to explicit palette
        if palette_ref not in palette_registry:
            raise NGResError(
                f"Visual asset '{name}' references unknown palette '{palette_ref}'"
            )
        palette_name = palette_ref
        palette = palette_registry[palette_ref]['colors']
        palette_idx = palette_registry[palette_ref]['index']

    elif isinstance(palette_ref, int):
        # Legacy: manual index, but still generate palette data
        palette = build_palette_from_frames(frames, max_colors=16, asset_name=name)
        palette_name = name
        # Register with specified index (may conflict - user's responsibility)
        if palette_name not in palette_registry:
            palette_registry[palette_name] = {
                'index': palette_ref,
                'colors': palette,
            }
        palette_idx = palette_ref

    else:
        raise NGResError(
            f"Visual asset '{name}': palette must be string (palette name) or int (index)"
        )

    # Convert all frames to tiles using the same palette
    # Tiles are organized in column-major order within each frame
    all_c1_data = bytearray()
    all_c2_data = bytearray()

    tiles_x = frame_width // 16
    tiles_y = frame_height // 16
    tiles_per_frame = tiles_x * tiles_y

    # Generate tilemap (row-major order for SDK)
    # For each frame, map[ty * width + tx] = tile_offset
    # C-ROM tiles are organized column-major within frame
    tilemap = []

    for frame_idx, frame in enumerate(frames):
        indexed = index_frame_with_palette(frame, palette)

        # Generate tiles in column-major order (for C-ROM)
        for tx in range(tiles_x):
            for ty in range(tiles_y):
                tile = []
                for py in range(16):
                    row = []
                    for px in range(16):
                        x = tx * 16 + px
                        y = ty * 16 + py
                        idx = y * frame_width + x
                        row.append(indexed[idx])
                    tile.append(row)

                c1, c2 = tile_to_crom(tile)
                all_c1_data.extend(c1)
                all_c2_data.extend(c2)

        # Generate tilemap entries for this frame (row-major)
        frame_base = frame_idx * tiles_per_frame
        for ty in range(tiles_y):
            for tx in range(tiles_x):
                # C-ROM tile offset = column-major within frame
                tile_offset = tx * tiles_y + ty
                # Add HFLIP flag for NeoGeo convention
                tilemap.append((frame_base + tile_offset) | 0x8000)

    total_tiles = frame_count * tiles_per_frame

    asset_info = {
        'name': name,
        'base_tile': base_tile,
        'width_pixels': frame_width,
        'height_pixels': frame_height,
        'width_tiles': tiles_x,
        'height_tiles': tiles_y,
        'tiles_per_frame': tiles_per_frame,
        'frame_count': frame_count,
        'animations': anim_defs,
        'palette_name': palette_name,  # Reference to palette in registry
        'palette_idx': palette_idx,
        'tilemap': tilemap,
    }

    return bytes(all_c1_data), bytes(all_c2_data), palette, asset_info, total_tiles


# ============================================================================
# Audio Asset Processing
# ============================================================================

def process_sound_effect(sfx_def, yaml_dir, index, current_offset):
    """
    Process a sound effect definition.
    Returns: (adpcm_data, sfx_info)
    """
    name = sfx_def.get('name')
    if not name:
        raise NGResError("Sound effect missing 'name' field")

    source = sfx_def.get('source')
    if not source:
        raise NGResError(f"Sound effect '{name}' missing 'source' field")

    # ADPCM-A uses fixed 18500 Hz sample rate
    ADPCM_A_RATE = 18500

    samples, orig_rate, source_path = load_wav_file(source, yaml_dir, ADPCM_A_RATE)

    # Encode to ADPCM-A
    encoder = ADPCM_A()
    adpcm_nibbles = encoder.encode_s16(samples)
    adpcm_data = pack_adpcm(adpcm_nibbles)

    # Addresses are in 256-byte units (16-bit max = 16MB)
    start_addr = current_offset // 256
    stop_addr = (current_offset + len(adpcm_data) - 1) // 256

    if stop_addr > 0xFFFF:
        raise NGResError(f"Sound effect '{name}' exceeds V-ROM address limit. "
                        f"Total audio data ({stop_addr * 256} bytes) exceeds 16MB.")

    sfx_info = {
        'name': name,
        'index': index,
        'start_addr_l': start_addr & 0xFF,
        'start_addr_h': (start_addr >> 8) & 0xFF,
        'stop_addr_l': stop_addr & 0xFF,
        'stop_addr_h': (stop_addr >> 8) & 0xFF,
        'size': len(adpcm_data),
        'orig_rate': orig_rate,
    }

    return adpcm_data, sfx_info


def process_music(music_def, yaml_dir, index, current_offset):
    """
    Process a music definition.
    Returns: (adpcm_data, music_info)
    """
    name = music_def.get('name')
    if not name:
        raise NGResError("Music missing 'name' field")

    source = music_def.get('source')
    if not source:
        raise NGResError(f"Music '{name}' missing 'source' field")

    # ADPCM-B supports variable rate, default to 22050 Hz
    target_rate = music_def.get('sample_rate', 22050)

    samples, _, source_path = load_wav_file(source, yaml_dir, target_rate)

    # Encode to ADPCM-B
    encoder = ADPCM_B()
    adpcm_nibbles = encoder.encode_s16(samples)
    adpcm_data = pack_adpcm(adpcm_nibbles)

    # Calculate Delta-N for playback rate
    delta_n = calculate_delta_n(target_rate)

    # Addresses are in 256-byte units (16-bit max = 16MB)
    start_addr = current_offset // 256
    stop_addr = (current_offset + len(adpcm_data) - 1) // 256

    if stop_addr > 0xFFFF:
        raise NGResError(f"Music '{name}' exceeds V-ROM address limit. "
                        f"Total audio data ({stop_addr * 256} bytes) exceeds 16MB.")

    music_info = {
        'name': name,
        'index': index,
        'start_addr_l': start_addr & 0xFF,
        'start_addr_h': (start_addr >> 8) & 0xFF,
        'stop_addr_l': stop_addr & 0xFF,
        'stop_addr_h': (stop_addr >> 8) & 0xFF,
        'delta_n_l': delta_n & 0xFF,
        'delta_n_h': (delta_n >> 8) & 0xFF,
        'size': len(adpcm_data),
        'sample_rate': target_rate,
    }

    return adpcm_data, music_info


# ============================================================================
# Tilemap Asset Processing (TMX format from Tiled editor)
# ============================================================================

def parse_tmx_file(tmx_path, layer_name=None):
    """
    Parse a TMX file from Tiled editor.
    Returns: (width, height, tile_data, collision_data, tileset_firstgid)

    TMX format is XML with structure:
    <map>
      <tileset firstgid="1" source="tileset.tsx"/>
      <layer name="Ground" width="100" height="14">
        <data encoding="csv">1,2,3,...</data>
      </layer>
    </map>
    """
    try:
        tree = ET.parse(tmx_path)
        root = tree.getroot()
    except ET.ParseError as e:
        raise NGResError(f"Failed to parse TMX file {tmx_path}: {e}")

    # Get map dimensions
    map_width = int(root.get('width', 0))
    map_height = int(root.get('height', 0))
    tile_width = int(root.get('tilewidth', 16))
    tile_height = int(root.get('tileheight', 16))

    if tile_width != 16 or tile_height != 16:
        raise NGResError(f"TMX file {tmx_path}: tile size must be 16x16 (got {tile_width}x{tile_height})")

    # Find tileset and get firstgid (the ID offset for this tileset)
    tileset_firstgid = 1
    tileset_elem = root.find('tileset')
    if tileset_elem is not None:
        tileset_firstgid = int(tileset_elem.get('firstgid', 1))

    # Find the requested layer (or first layer if none specified)
    layer = None
    for elem in root.findall('layer'):
        if layer_name is None or elem.get('name') == layer_name:
            layer = elem
            break

    if layer is None:
        if layer_name:
            raise NGResError(f"TMX file {tmx_path}: layer '{layer_name}' not found")
        else:
            raise NGResError(f"TMX file {tmx_path}: no tile layers found")

    # Get layer dimensions (may differ from map)
    layer_width = int(layer.get('width', map_width))
    layer_height = int(layer.get('height', map_height))

    # Parse tile data
    data_elem = layer.find('data')
    if data_elem is None:
        raise NGResError(f"TMX file {tmx_path}: layer has no data element")

    encoding = data_elem.get('encoding', 'csv')
    if encoding != 'csv':
        raise NGResError(f"TMX file {tmx_path}: only CSV encoding supported (got '{encoding}')")

    # Parse CSV data - tile IDs are 1-based in Tiled, 0 means empty
    csv_text = data_elem.text.strip()
    tile_ids = []
    for val in csv_text.replace('\n', ',').split(','):
        val = val.strip()
        if val:
            tile_id = int(val)
            # Subtract firstgid to get 0-based tile index
            # 0 in TMX means empty/transparent
            if tile_id == 0:
                tile_ids.append(0)
            else:
                tile_ids.append(tile_id - tileset_firstgid)

    if len(tile_ids) != layer_width * layer_height:
        raise NGResError(
            f"TMX file {tmx_path}: expected {layer_width * layer_height} tiles, got {len(tile_ids)}"
        )

    # Convert to bytes (tile indices as u8)
    tile_data = bytes([min(255, max(0, t)) for t in tile_ids])

    # Extract collision data from tile properties (if present)
    # Look for tileset with tile properties
    collision_data = None
    collision_map = {}  # tile_id -> collision flags

    for tileset in root.findall('tileset'):
        for tile in tileset.findall('tile'):
            tile_id = int(tile.get('id', 0))
            props = tile.find('properties')
            if props is not None:
                flags = 0
                for prop in props.findall('property'):
                    prop_name = prop.get('name', '').lower()
                    prop_value = prop.get('value', 'false').lower()
                    if prop_value in ('true', '1', 'yes'):
                        if prop_name == 'solid':
                            flags |= 0x01
                        elif prop_name == 'platform':
                            flags |= 0x02
                        elif prop_name == 'slope_l':
                            flags |= 0x04
                        elif prop_name == 'slope_r':
                            flags |= 0x08
                        elif prop_name == 'hazard':
                            flags |= 0x10
                        elif prop_name == 'trigger':
                            flags |= 0x20
                        elif prop_name == 'ladder':
                            flags |= 0x40
                if flags:
                    collision_map[tile_id] = flags

    # Build collision data array if we have any collision properties
    if collision_map:
        collision_bytes = []
        for tile_id in tile_ids:
            collision_bytes.append(collision_map.get(tile_id, 0))
        collision_data = bytes(collision_bytes)

    return layer_width, layer_height, tile_data, collision_data, tileset_firstgid


def process_tilemap_asset(tilemap_def, yaml_dir, visual_assets_info):
    """
    Process a tilemap asset definition.
    Returns: tilemap_info dict

    tilemap_def format:
      - name: level1_ground
        source: assets/levels/level1.tmx
        layer: "Ground"  # optional, uses first layer if omitted
        tileset: level_tileset  # reference to visual asset for tileset
        tileset_palettes:
          - tiles: [0, 31]
            palette: 5
        default_palette: 5
        collision:  # optional, override TMX tile properties
          solid: [1, 2, 3]
    """
    name = tilemap_def.get('name')
    if not name:
        raise NGResError("Tilemap missing 'name' field")

    source = tilemap_def.get('source')
    if not source:
        raise NGResError(f"Tilemap '{name}' missing 'source' field")

    # Resolve path
    if not os.path.isabs(source):
        source = os.path.join(yaml_dir, source)

    if not os.path.exists(source):
        raise NGResError(f"Tilemap '{name}' source not found: {source}")

    # Parse TMX file
    layer_name = tilemap_def.get('layer')
    width, height, tile_data, collision_data, firstgid = parse_tmx_file(source, layer_name)

    # Find tileset visual asset to get base_tile
    tileset_ref = tilemap_def.get('tileset')
    base_tile = 0
    if tileset_ref:
        for asset in visual_assets_info:
            if asset['name'] == tileset_ref:
                base_tile = asset['base_tile']
                break
        else:
            raise NGResError(f"Tilemap '{name}' references unknown tileset '{tileset_ref}'")

    # Process tileset_palettes to build lookup table
    tileset_palettes = tilemap_def.get('tileset_palettes', [])
    default_palette = tilemap_def.get('default_palette', 0)

    # Build tile_to_palette lookup table (256 entries)
    tile_to_palette = [default_palette] * 256
    for pal_range in tileset_palettes:
        tiles_range = pal_range.get('tiles', [0, 0])
        palette = pal_range.get('palette', default_palette)
        if len(tiles_range) == 2:
            start, end = tiles_range
            for i in range(start, min(end + 1, 256)):
                tile_to_palette[i] = palette

    # Process collision overrides from YAML (if no TMX collision or to override)
    collision_config = tilemap_def.get('collision')
    if collision_config:
        # Build collision data from YAML config
        collision_map = {}
        for coll_type, tile_indices in collision_config.items():
            flag = 0
            if coll_type == 'solid':
                flag = 0x01
            elif coll_type == 'platform':
                flag = 0x02
            elif coll_type == 'slope_l':
                flag = 0x04
            elif coll_type == 'slope_r':
                flag = 0x08
            elif coll_type == 'hazard':
                flag = 0x10
            elif coll_type == 'trigger':
                flag = 0x20
            elif coll_type == 'ladder':
                flag = 0x40

            if flag and tile_indices:
                for idx in tile_indices:
                    collision_map[idx] = collision_map.get(idx, 0) | flag

        if collision_map:
            collision_bytes = []
            for tile_id in tile_data:
                collision_bytes.append(collision_map.get(tile_id, 0))
            collision_data = bytes(collision_bytes)

    tilemap_info = {
        'name': name,
        'width_tiles': width,
        'height_tiles': height,
        'base_tile': base_tile,
        'tile_data': tile_data,
        'collision_data': collision_data,
        'tile_to_palette': bytes(tile_to_palette),
        'default_palette': default_palette,
    }

    return tilemap_info


def generate_z80_sample_tables(sfx_info_list, music_info_list):
    """
    Generate binary sample tables for Z80 driver.
    Returns: bytes to be written at a known offset in M-ROM

    SFX table format (32 entries, 4 bytes each):
        start_addr_l, start_addr_h, stop_addr_l, stop_addr_h

    Music table format (32 entries, 6 bytes each):
        start_addr_l, start_addr_h, stop_addr_l, stop_addr_h, delta_n_l, delta_n_h
    """
    # SFX table: 32 entries * 4 bytes = 128 bytes
    sfx_table = bytearray(128)
    for sfx in sfx_info_list:
        idx = sfx['index']
        if idx < 32:
            offset = idx * 4
            sfx_table[offset] = sfx['start_addr_l']
            sfx_table[offset + 1] = sfx['start_addr_h']
            sfx_table[offset + 2] = sfx['stop_addr_l']
            sfx_table[offset + 3] = sfx['stop_addr_h']

    # Music table: 32 entries * 6 bytes = 192 bytes
    music_table = bytearray(192)
    for music in music_info_list:
        idx = music['index']
        if idx < 32:
            offset = idx * 6
            music_table[offset] = music['start_addr_l']
            music_table[offset + 1] = music['start_addr_h']
            music_table[offset + 2] = music['stop_addr_l']
            music_table[offset + 3] = music['stop_addr_h']
            music_table[offset + 4] = music['delta_n_l']
            music_table[offset + 5] = music['delta_n_h']

    return bytes(sfx_table + music_table)


def generate_header(assets_info, palette_registry, sfx_info, music_info, tilemap_info, output_path):
    """Generate C header file with asset definitions."""
    # Check if SDK UI assets are present (needed to decide on includes)
    asset_names = {asset['name'] for asset in assets_info}
    sfx_names = {sfx['name'] for sfx in sfx_info}
    has_sdk_ui_wrappers = (
        'progearsdk_ui_panel' in asset_names and
        'progearsdk_ui_cursor' in asset_names
    )

    lines = [
        "// ngres_generated_assets.h - Generated by ngres",
        "// DO NOT EDIT - This file is auto-generated from assets.yaml",
        "",
        "#ifndef _NGRES_GENERATED_ASSETS_H_",
        "#define _NGRES_GENERATED_ASSETS_H_",
        "",
        "#include <visual.h>",
        "#include <palette.h>",
        "#include <audio.h>",
        "#include <tilemap.h>",
    ]

    # Include ui.h if we're generating SDK UI wrapper functions
    if has_sdk_ui_wrappers:
        lines.append("#include <ui.h>")

    lines.append("")

    # === Palette Index Constants ===
    # Sort palettes by index for consistent output
    palette_items = [
        (name, info) for name, info in palette_registry.items()
        if not name.startswith('_')  # Skip internal keys
    ]
    palette_items.sort(key=lambda x: x[1]['index'])

    if palette_items:
        lines.append("// === Palette Index Constants ===")
        for pal_name, pal_info in palette_items:
            const_name = f"NGPAL_{pal_name.upper()}"
            lines.append(f"#define {const_name} {pal_info['index']}")
        lines.append("")

    # === Palette Data Arrays ===
    if palette_items:
        lines.append("// === Palette Data ===")
        for pal_name, pal_info in palette_items:
            colors = pal_info['colors']
            lines.append(f"static const u16 NGPal_{pal_name}[16] = {{")
            pal_line = "    "
            for i in range(16):
                if i < len(colors):
                    r, g, b = colors[i]
                    color = rgb5_to_neogeo_color(r, g, b)
                    if i == 0:
                        color = 0x8000  # Reference/transparent
                else:
                    color = 0x0000
                pal_line += f"0x{color:04X}, "
            lines.append(pal_line)
            lines.append("};")
            lines.append("")

    # === Visual Asset Definitions ===
    lines.append("// === Visual Assets ===")
    lines.append("")

    for asset in assets_info:
        name = asset['name']
        name_upper = name.upper()
        palette_name = asset['palette_name']

        # Animation enum (if has animations)
        if asset['animations']:
            lines.append(f"// Animation indices for {name}")
            lines.append("enum {")
            for i, anim in enumerate(asset['animations']):
                anim_name = anim['name'].upper()
                lines.append(f"    NG_ANIM_{name_upper}_{anim_name} = {i},")
            lines.append("};")
            lines.append("")

        # Animation definitions array
        if asset['animations']:
            lines.append(f"static const NGAnimDef _{name}_anims[] = {{")
            for anim in asset['animations']:
                lines.append(
                    f"    {{ \"{anim['name']}\", {anim['first_frame']}, "
                    f"{anim['frame_count']}, {anim['speed']}, {anim['loop']} }},"
                )
            lines.append("};")
            lines.append("")

        # Tilemap array
        tilemap = asset['tilemap']
        first_frame_tiles = asset['tiles_per_frame']
        lines.append(f"static const u16 _{name}_tilemap[] = {{")
        for i in range(0, min(len(tilemap), first_frame_tiles), 16):
            chunk = tilemap[i:min(i+16, first_frame_tiles)]
            line = "    " + ", ".join(f"0x{t:04X}" for t in chunk) + ","
            lines.append(line)
        lines.append("};")
        lines.append("")

        # NGVisualAsset struct - now with palette_data
        lines.append(f"static const NGVisualAsset NGVisualAsset_{name} = {{")
        lines.append(f"    .name = \"{name}\",")
        lines.append(f"    .base_tile = {asset['base_tile']},")
        lines.append(f"    .width_pixels = {asset['width_pixels']},")
        lines.append(f"    .height_pixels = {asset['height_pixels']},")
        lines.append(f"    .width_tiles = {asset['width_tiles']},")
        lines.append(f"    .height_tiles = {asset['height_tiles']},")
        lines.append(f"    .tilemap = _{name}_tilemap,")
        lines.append(f"    .palette = NGPAL_{palette_name.upper()},")
        lines.append(f"    .palette_data = NGPal_{palette_name},")
        if asset['animations']:
            lines.append(f"    .anims = _{name}_anims,")
            lines.append(f"    .anim_count = {len(asset['animations'])},")
        else:
            lines.append("    .anims = 0,")
            lines.append("    .anim_count = 0,")
        lines.append(f"    .frame_count = {asset['frame_count']},")
        lines.append(f"    .tiles_per_frame = {asset['tiles_per_frame']},")
        lines.append("};")
        lines.append("")

    # === NGPalInitAssets Function ===
    # Generated with __attribute__((weak)) so multiple inclusions don't cause
    # linker errors, but it still overrides the empty weak default in engine.c
    if palette_items:
        lines.append("// === Palette Initialization ===")
        lines.append("// Called automatically by NGEngineInit() to load all asset palettes")
        lines.append("__attribute__((weak)) void NGPalInitAssets(void) {")
        for pal_name, pal_info in palette_items:
            const_name = f"NGPAL_{pal_name.upper()}"
            lines.append(f"    NGPalSet({const_name}, NGPal_{pal_name});")
        lines.append("}")
        lines.append("")

    # === Sound Effect Assets ===
    if sfx_info:
        lines.append("// === Sound Effects ===")
        lines.append("")

        # SFX index constants
        for sfx in sfx_info:
            lines.append(f"#define NGSFX_{sfx['name'].upper()} {sfx['index']}")
        lines.append("")

        # SFX asset definitions
        for sfx in sfx_info:
            lines.append(f"static const NGSfxAsset NGSfxAsset_{sfx['name']} = {{")
            lines.append(f"    .name = \"{sfx['name']}\",")
            lines.append(f"    .index = {sfx['index']},")
            lines.append("};")
            lines.append("")

    # === Music Assets ===
    if music_info:
        lines.append("// === Music ===")
        lines.append("")

        # Music index constants
        for music in music_info:
            lines.append(f"#define NGMUSIC_{music['name'].upper()} {music['index']}")
        lines.append("")

        # Music asset definitions
        for music in music_info:
            lines.append(f"static const NGMusicAsset NGMusicAsset_{music['name']} = {{")
            lines.append(f"    .name = \"{music['name']}\",")
            lines.append(f"    .index = {music['index']},")
            lines.append("};")
            lines.append("")

    # === Tilemap Assets ===
    if tilemap_info:
        lines.append("// === Tilemaps ===")
        lines.append("")

        for tm in tilemap_info:
            name = tm['name']

            # Tile data array
            tile_data = tm['tile_data']
            lines.append(f"static const u8 _{name}_tile_data[] = {{")
            for i in range(0, len(tile_data), 32):
                chunk = tile_data[i:i+32]
                line = "    " + ", ".join(f"0x{b:02X}" for b in chunk) + ","
                lines.append(line)
            lines.append("};")
            lines.append("")

            # Collision data array (if present)
            collision_data = tm['collision_data']
            if collision_data:
                lines.append(f"static const u8 _{name}_collision_data[] = {{")
                for i in range(0, len(collision_data), 32):
                    chunk = collision_data[i:i+32]
                    line = "    " + ", ".join(f"0x{b:02X}" for b in chunk) + ","
                    lines.append(line)
                lines.append("};")
                lines.append("")

            # Tile to palette lookup table
            tile_to_palette = tm['tile_to_palette']
            lines.append(f"static const u8 _{name}_tile_to_palette[] = {{")
            for i in range(0, len(tile_to_palette), 32):
                chunk = tile_to_palette[i:i+32]
                line = "    " + ", ".join(f"{b}" for b in chunk) + ","
                lines.append(line)
            lines.append("};")
            lines.append("")

            # NGTilemapAsset struct
            lines.append(f"static const NGTilemapAsset NGTilemapAsset_{name} = {{")
            lines.append(f"    .name = \"{name}\",")
            lines.append(f"    .width_tiles = {tm['width_tiles']},")
            lines.append(f"    .height_tiles = {tm['height_tiles']},")
            lines.append(f"    .base_tile = {tm['base_tile']},")
            lines.append(f"    .tile_data = _{name}_tile_data,")
            if collision_data:
                lines.append(f"    .collision_data = _{name}_collision_data,")
            else:
                lines.append("    .collision_data = 0,")
            lines.append(f"    .tile_to_palette = _{name}_tile_to_palette,")
            lines.append(f"    .default_palette = {tm['default_palette']},")
            lines.append("};")
            lines.append("")

    # === SDK Default Wrapper Functions ===
    # Generate convenience wrappers if SDK UI assets are present
    asset_names = {asset['name'] for asset in assets_info}
    sfx_names = {sfx['name'] for sfx in sfx_info}

    has_sdk_ui_panel = 'progearsdk_ui_panel' in asset_names
    has_sdk_ui_cursor = 'progearsdk_ui_cursor' in asset_names
    has_sdk_ui_click = 'progearsdk_ui_click' in sfx_names
    has_sdk_ui_select = 'progearsdk_ui_select' in sfx_names

    if has_sdk_ui_panel and has_sdk_ui_cursor:
        lines.append("// === SDK Default UI Wrappers ===")
        lines.append("")
        lines.append("/**")
        lines.append(" * Create a menu using SDK default panel and cursor assets.")
        lines.append(" * @param arena Arena to allocate from (typically ng_arena_state)")
        lines.append(" * @param dim_amount Background dimming intensity (0=none, 1-31=darken)")
        lines.append(" * @return Menu handle, or NULL if allocation failed")
        lines.append(" */")
        lines.append("static inline NGMenuHandle NGMenuCreateDefault(NGArena *arena, u8 dim_amount) {")
        lines.append("    return NGMenuCreate(arena, &NGVisualAsset_progearsdk_ui_panel,")
        lines.append("                        &NGVisualAsset_progearsdk_ui_cursor, dim_amount);")
        lines.append("}")
        lines.append("")

    if has_sdk_ui_click and has_sdk_ui_select:
        lines.append("/**")
        lines.append(" * Set SDK default sound effects for menu interactions.")
        lines.append(" * Uses progearsdk_ui_click (move) and progearsdk_ui_select sounds.")
        lines.append(" * @param menu Menu handle")
        lines.append(" */")
        lines.append("static inline void NGMenuSetDefaultSounds(NGMenuHandle menu) {")
        lines.append("    NGMenuSetSounds(menu, NGSFX_PROGEARSDK_UI_CLICK, NGSFX_PROGEARSDK_UI_SELECT);")
        lines.append("}")
        lines.append("")

    lines.append("#endif // _NGRES_GENERATED_ASSETS_H_")
    lines.append("")

    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))


def load_yaml_config(yaml_path):
    """
    Load a YAML config file and resolve all source paths to absolute paths.
    Returns: (config dict, yaml_dir Path)
    """
    yaml_path = Path(yaml_path)
    if not yaml_path.exists():
        raise NGResError(f"YAML file not found: {yaml_path}")

    yaml_dir = yaml_path.parent

    with open(yaml_path) as f:
        try:
            config = yaml.safe_load(f)
        except yaml.YAMLError as e:
            raise NGResError(f"Invalid YAML in {yaml_path}: {e}")

    if not config:
        return {}, yaml_dir

    # Resolve all source paths to absolute paths
    # This allows merging configs from different directories

    # Resolve palette sources
    for pal_name, pal_def in config.get('palettes', {}).items():
        if 'source' in pal_def and not os.path.isabs(pal_def['source']):
            pal_def['source'] = str(yaml_dir / pal_def['source'])

    # Resolve visual asset sources
    for asset in config.get('visual_assets', []):
        if 'source' in asset and not os.path.isabs(asset['source']):
            asset['source'] = str(yaml_dir / asset['source'])

    # Resolve sound effect sources
    for sfx in config.get('sound_effects', []):
        if 'source' in sfx and not os.path.isabs(sfx['source']):
            sfx['source'] = str(yaml_dir / sfx['source'])

    # Resolve music sources
    for music in config.get('music', []):
        if 'source' in music and not os.path.isabs(music['source']):
            music['source'] = str(yaml_dir / music['source'])

    # Resolve tilemap sources
    for tilemap in config.get('tilemaps', []):
        if 'source' in tilemap and not os.path.isabs(tilemap['source']):
            tilemap['source'] = str(yaml_dir / tilemap['source'])

    return config, yaml_dir


def merge_configs(base_config, additional_config):
    """
    Merge two asset configs. Additional config is appended to base.
    SDK assets should be in base_config so they get processed first.
    """
    merged = {
        'palettes': {},
        'visual_assets': [],
        'sound_effects': [],
        'music': [],
        'tilemaps': [],
    }

    # Merge palettes (dict merge, additional overwrites base on conflict)
    merged['palettes'].update(base_config.get('palettes', {}))
    merged['palettes'].update(additional_config.get('palettes', {}))

    # Merge lists (base first, then additional)
    merged['visual_assets'] = (
        base_config.get('visual_assets', []) +
        additional_config.get('visual_assets', [])
    )
    merged['sound_effects'] = (
        base_config.get('sound_effects', []) +
        additional_config.get('sound_effects', [])
    )
    merged['music'] = (
        base_config.get('music', []) +
        additional_config.get('music', [])
    )
    merged['tilemaps'] = (
        base_config.get('tilemaps', []) +
        additional_config.get('tilemaps', [])
    )

    return merged


def main():
    parser = argparse.ArgumentParser(
        description='NeoGeo Resource Compiler - Process visual and audio assets from YAML',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('yaml_file', help='Input YAML file')
    parser.add_argument('--sdk-assets', help='SDK assets YAML file (processed before user assets)')
    parser.add_argument('-o', '--output', default='.', help='Output directory')
    parser.add_argument('--c1', default='sprites-c1.bin', help='C1 ROM output filename')
    parser.add_argument('--c2', default='sprites-c2.bin', help='C2 ROM output filename')
    parser.add_argument('--v1', default='audio-v1.bin', help='V1 ROM output filename (audio)')
    parser.add_argument('--m1-tables', default='audio-tables.bin', help='Z80 sample tables output')
    parser.add_argument('--header', default='ngres_generated_assets.h', help='Header output filename')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')

    args = parser.parse_args()

    # Load user YAML (required)
    try:
        user_config, yaml_dir = load_yaml_config(args.yaml_file)
    except NGResError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    # Load SDK assets YAML (optional)
    sdk_config = {}
    if args.sdk_assets:
        sdk_path = Path(args.sdk_assets)
        if sdk_path.exists():
            try:
                sdk_config, _ = load_yaml_config(args.sdk_assets)
                if args.verbose:
                    print(f"Loaded SDK assets from {args.sdk_assets}")
            except NGResError as e:
                print(f"Error loading SDK assets: {e}", file=sys.stderr)
                sys.exit(1)
        elif args.verbose:
            print(f"SDK assets file not found: {args.sdk_assets}, skipping")

    # Merge configs (SDK first, then user)
    if sdk_config:
        config = merge_configs(sdk_config, user_config)
    else:
        config = user_config

    if not config:
        print("Error: No assets to process", file=sys.stderr)
        sys.exit(1)

    visual_assets = config.get('visual_assets', [])
    palettes_config = config.get('palettes', {})
    sound_effects_config = config.get('sound_effects', [])
    music_config = config.get('music', [])
    tilemaps_config = config.get('tilemaps', [])

    # Initialize palette registry
    # Indices 0-1 reserved for system, start auto-assignment at 2
    palette_registry = {
        '_next_index': 2,  # Internal: next auto-assigned index
    }

    # Process explicit palette definitions first
    for pal_name, pal_def in palettes_config.items():
        try:
            colors = process_palette_def(pal_name, pal_def, yaml_dir)
            palette_registry[pal_name] = {
                'index': palette_registry['_next_index'],
                'colors': colors,
            }
            palette_registry['_next_index'] += 1
            if args.verbose:
                print(f"Processed palette '{pal_name}': index {palette_registry[pal_name]['index']}")
        except NGResError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)

    # Process all assets
    # Eyecatcher uses tiles 256-319 (bank 1), user tiles start at tile 320
    TILE_START = 320
    TILE_SIZE = 64  # bytes per tile
    EYECATCHER_TILES = 64  # tiles reserved for eyecatcher (256-319)

    # Pre-allocate space for reserved tiles (0-319)
    all_c1_data = bytearray(TILE_START * TILE_SIZE)
    all_c2_data = bytearray(TILE_START * TILE_SIZE)

    # Load eyecatcher tiles from SDK rom directory
    # Eyecatcher tiles go in bank 1 (tiles 256-319), matching mslugx layout
    script_dir = os.path.dirname(os.path.abspath(__file__))
    sdk_rom_dir = os.path.join(script_dir, '..', 'sdk', 'rom')
    eyecatcher_c1_path = os.path.join(sdk_rom_dir, 'eyecatcher-c1.bin')
    eyecatcher_c2_path = os.path.join(sdk_rom_dir, 'eyecatcher-c2.bin')

    EYECATCHER_OFFSET = 256 * TILE_SIZE  # Bank 1 starts at tile 256

    if os.path.exists(eyecatcher_c1_path) and os.path.exists(eyecatcher_c2_path):
        with open(eyecatcher_c1_path, 'rb') as f:
            eyecatcher_c1 = f.read()
        with open(eyecatcher_c2_path, 'rb') as f:
            eyecatcher_c2 = f.read()
        # Copy eyecatcher tiles to bank 1 (tiles 256-319)
        all_c1_data[EYECATCHER_OFFSET:EYECATCHER_OFFSET + len(eyecatcher_c1)] = eyecatcher_c1
        all_c2_data[EYECATCHER_OFFSET:EYECATCHER_OFFSET + len(eyecatcher_c2)] = eyecatcher_c2
        if args.verbose:
            print(f"Loaded eyecatcher tiles at bank 1: {len(eyecatcher_c1)} bytes")

    assets_info = []
    base_tile = TILE_START

    # Process visual assets
    for asset_def in visual_assets:
        try:
            c1, c2, palette, info, tile_count = process_visual_asset(
                asset_def, yaml_dir, base_tile, palette_registry
            )
            all_c1_data.extend(c1)
            all_c2_data.extend(c2)
            assets_info.append(info)

            if args.verbose:
                print(f"Processed '{info['name']}': "
                      f"{info['width_pixels']}x{info['height_pixels']}, "
                      f"{info['frame_count']} frames, {tile_count} tiles")

            base_tile += tile_count

        except NGResError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)

    # =========================================================================
    # Process Audio Assets
    # =========================================================================
    all_v1_data = bytearray()
    sfx_info_list = []
    music_info_list = []
    audio_offset = 0

    # Process sound effects
    for i, sfx_def in enumerate(sound_effects_config):
        if i >= 32:
            print(f"Warning: Maximum 32 sound effects supported, ignoring extras", file=sys.stderr)
            break
        try:
            adpcm_data, sfx_info = process_sound_effect(sfx_def, yaml_dir, i, audio_offset)
            all_v1_data.extend(adpcm_data)
            sfx_info_list.append(sfx_info)
            audio_offset += len(adpcm_data)

            if args.verbose:
                print(f"Processed SFX '{sfx_info['name']}': {sfx_info['size']} bytes")

        except NGResError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)

    # Process music
    for i, music_def in enumerate(music_config):
        if i >= 32:
            print(f"Warning: Maximum 32 music tracks supported, ignoring extras", file=sys.stderr)
            break
        try:
            adpcm_data, music_info = process_music(music_def, yaml_dir, i, audio_offset)
            all_v1_data.extend(adpcm_data)
            music_info_list.append(music_info)
            audio_offset += len(adpcm_data)

            if args.verbose:
                print(f"Processed Music '{music_info['name']}': {music_info['size']} bytes @ {music_info['sample_rate']}Hz")

        except NGResError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)

    # =========================================================================
    # Process Tilemap Assets
    # =========================================================================
    tilemap_info_list = []

    for tilemap_def in tilemaps_config:
        try:
            tilemap_info = process_tilemap_asset(tilemap_def, yaml_dir, assets_info)
            tilemap_info_list.append(tilemap_info)

            if args.verbose:
                print(f"Processed Tilemap '{tilemap_info['name']}': "
                      f"{tilemap_info['width_tiles']}x{tilemap_info['height_tiles']} tiles")

        except NGResError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)

    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    # Write C-ROM data
    c1_path = output_dir / args.c1
    c2_path = output_dir / args.c2

    # Pad to minimum size (64KB each)
    min_size = 64 * 1024
    while len(all_c1_data) < min_size:
        all_c1_data.append(0)
    while len(all_c2_data) < min_size:
        all_c2_data.append(0)

    with open(c1_path, 'wb') as f:
        f.write(all_c1_data)

    with open(c2_path, 'wb') as f:
        f.write(all_c2_data)

    # Write V-ROM data (if any audio)
    v1_path = output_dir / args.v1
    m1_tables_path = output_dir / args.m1_tables
    if sfx_info_list or music_info_list:
        # Pad to minimum 512KB (typical V-ROM size)
        min_v_size = 512 * 1024
        while len(all_v1_data) < min_v_size:
            all_v1_data.append(0)

        with open(v1_path, 'wb') as f:
            f.write(all_v1_data)

        # Write Z80 sample tables
        sample_tables = generate_z80_sample_tables(sfx_info_list, music_info_list)
        with open(m1_tables_path, 'wb') as f:
            f.write(sample_tables)

    # Generate header
    header_path = output_dir / args.header
    generate_header(assets_info, palette_registry, sfx_info_list, music_info_list, tilemap_info_list, header_path)

    # Count palettes (excluding internal keys)
    palette_count = len([k for k in palette_registry.keys() if not k.startswith('_')])

    print(f"Generated:")
    print(f"  {c1_path} ({len(all_c1_data)} bytes)")
    print(f"  {c2_path} ({len(all_c2_data)} bytes)")
    if sfx_info_list or music_info_list:
        print(f"  {v1_path} ({len(all_v1_data)} bytes)")
    print(f"  {header_path}")
    print(f"Total: {base_tile} tiles, {palette_count} palettes, {len(assets_info)} visual assets")
    if sfx_info_list:
        print(f"       {len(sfx_info_list)} sound effects")
    if music_info_list:
        print(f"       {len(music_info_list)} music tracks")
    if tilemap_info_list:
        print(f"       {len(tilemap_info_list)} tilemaps")


if __name__ == '__main__':
    main()
