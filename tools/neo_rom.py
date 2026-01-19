#!/usr/bin/env python3
"""
neo_rom.py - Generate .neo ROM files for NeoSD/NeoSD Pro flash cartridges

The .neo format is a simple container with a 4KB header followed by raw ROM data.
This tool takes individual ROM files (P, S, M, V, C1, C2) and packages them into
a single .neo file.

Usage:
    python3 neo_rom.py -o output.neo \
        --p1 mygame-p1.bin \
        --s1 mygame-s1.bin \
        --m1 mygame-m1.bin \
        --v1 mygame-v1.bin \
        --c1 mygame-c1.bin \
        --c2 mygame-c2.bin \
        --name "My Game" \
        --manufacturer "My Company" \
        --year 2024 \
        --ngh 9999
"""

import argparse
import struct
import os
import sys


# Neo file header magic bytes
NEO_HEADER_MAGIC = b'NEO'
NEO_VERSION = 1

# Header size is always 4KB
HEADER_SIZE = 4096

# Standard NeoGeo ROM sizes for padding
STD_P_SIZE = 524288    # 512KB
STD_S_SIZE = 131072    # 128KB
STD_M_SIZE = 131072    # 128KB
STD_V_SIZE = 2097152   # 2MB
STD_C_SIZE = 2097152   # 2MB (interleaved C1+C2, 1MB each)


def pad_to_size(data, target_size):
    """Pad data with zeros to reach target size."""
    if len(data) >= target_size:
        return data
    return data + b'\x00' * (target_size - len(data))


def read_rom_file(path):
    """Read a ROM file, returning empty bytes if file doesn't exist or path is None."""
    if path is None or not os.path.exists(path):
        return b''
    with open(path, 'rb') as f:
        return f.read()


def interleave_c_roms(c1_data, c2_data):
    """
    Interleave C1 and C2 ROM data for neo format.

    NeoGeo C-ROMs store sprite data split across two chips.
    The neo format expects them interleaved byte-by-byte: C1[0], C2[0], C1[1], C2[1], ...
    """
    # Ensure both are the same length
    max_len = max(len(c1_data), len(c2_data))
    c1_data = c1_data.ljust(max_len, b'\x00')
    c2_data = c2_data.ljust(max_len, b'\x00')

    # Interleave: C1[0], C2[0], C1[1], C2[1], ...
    result = bytearray(max_len * 2)
    for i in range(max_len):
        result[i * 2] = c1_data[i]      # Even bytes from C1
        result[i * 2 + 1] = c2_data[i]  # Odd bytes from C2

    return bytes(result)


def build_neo_header(p_size, s_size, m_size, v1_size, v2_size, c_size,
                     name, manufacturer, year, genre, screenshot, ngh):
    """
    Build the 4KB neo file header.

    Header structure:
        uint8_t header1, header2, header3, version;  // "NEO" + version
        uint32_t PSize, SSize, MSize, V1Size, V2Size, CSize;
        uint32_t Year;
        uint32_t Genre;
        uint32_t Screenshot;
        uint32_t NGH;
        uint8_t Name[33];
        uint8_t Manu[17];
        uint8_t Filler[418];   // fill to 512
        uint8_t Filler2[3584]; // fill to 4096
    """
    header = bytearray(HEADER_SIZE)

    # Magic bytes and version
    header[0:3] = NEO_HEADER_MAGIC
    header[3] = NEO_VERSION

    # ROM sizes (little-endian uint32)
    offset = 4
    for size in [p_size, s_size, m_size, v1_size, v2_size, c_size]:
        struct.pack_into('<I', header, offset, size)
        offset += 4

    # Metadata
    struct.pack_into('<I', header, offset, year)
    offset += 4
    struct.pack_into('<I', header, offset, genre)
    offset += 4
    struct.pack_into('<I', header, offset, screenshot)
    offset += 4
    struct.pack_into('<I', header, offset, ngh)
    offset += 4

    # Name (33 bytes, null-terminated)
    name_bytes = name.encode('ascii', errors='replace')[:32]
    header[offset:offset + len(name_bytes)] = name_bytes
    offset += 33

    # Manufacturer (17 bytes, null-terminated)
    manu_bytes = manufacturer.encode('ascii', errors='replace')[:16]
    header[offset:offset + len(manu_bytes)] = manu_bytes

    # Rest is already zero-filled
    return bytes(header)


def build_neo_file(output_path, p1_path, s1_path, m1_path, v1_path, c1_path, c2_path,
                   name, manufacturer, year, genre, screenshot, ngh, verbose=False):
    """Build a complete .neo file from individual ROM files."""

    # Read all ROM files
    p_data = read_rom_file(p1_path)
    s_data = read_rom_file(s1_path)
    m_data = read_rom_file(m1_path)
    v1_data = read_rom_file(v1_path)
    c1_data = read_rom_file(c1_path)
    c2_data = read_rom_file(c2_path)

    # Pad ROMs to standard NeoGeo sizes
    p_data = pad_to_size(p_data, STD_P_SIZE)
    s_data = pad_to_size(s_data, STD_S_SIZE)
    m_data = pad_to_size(m_data, STD_M_SIZE)
    v1_data = pad_to_size(v1_data, STD_V_SIZE)

    # Pad C1/C2 to 1MB each before interleaving (results in 2MB interleaved)
    c1_data = pad_to_size(c1_data, STD_C_SIZE // 2)
    c2_data = pad_to_size(c2_data, STD_C_SIZE // 2)

    # Interleave C1 and C2 for neo format
    c_data = interleave_c_roms(c1_data, c2_data)

    # V2 is always 0 for homebrew (merged ADPCM-A/B)
    v2_size = 0

    if verbose:
        print(f"ROM sizes (padded to standard):")
        print(f"  P-ROM: {len(p_data):,} bytes")
        print(f"  S-ROM: {len(s_data):,} bytes")
        print(f"  M-ROM: {len(m_data):,} bytes")
        print(f"  V1-ROM: {len(v1_data):,} bytes")
        print(f"  V2-ROM: {v2_size:,} bytes (not used for homebrew)")
        print(f"  C-ROM: {len(c_data):,} bytes (interleaved C1+C2)")

    # Build header
    header = build_neo_header(
        p_size=len(p_data),
        s_size=len(s_data),
        m_size=len(m_data),
        v1_size=len(v1_data),
        v2_size=v2_size,
        c_size=len(c_data),
        name=name,
        manufacturer=manufacturer,
        year=year,
        genre=genre,
        screenshot=screenshot,
        ngh=ngh
    )

    # Write output file
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(p_data)
        f.write(s_data)
        f.write(m_data)
        f.write(v1_data)
        # V2 data would go here, but size is 0
        f.write(c_data)

    total_size = HEADER_SIZE + len(p_data) + len(s_data) + len(m_data) + len(v1_data) + len(c_data)

    if verbose:
        print(f"\nCreated: {output_path}")
        print(f"Total size: {total_size:,} bytes ({total_size / 1024 / 1024:.2f} MB)")

    return total_size


def main():
    parser = argparse.ArgumentParser(
        description='Generate .neo ROM files for NeoSD/NeoSD Pro flash cartridges',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s -o game.neo --p1 game-p1.bin --s1 game-s1.bin --m1 game-m1.bin \\
           --v1 game-v1.bin --c1 game-c1.bin --c2 game-c2.bin \\
           --name "My Game" --year 2024

  %(prog)s -o test.neo --p1 game-p1.bin --name "Quick Test"
           (creates neo with only P-ROM for quick testing)
"""
    )

    parser.add_argument('-o', '--output', required=True,
                        help='Output .neo file path')

    # ROM file inputs
    parser.add_argument('--p1', metavar='FILE',
                        help='P-ROM (program) file')
    parser.add_argument('--s1', metavar='FILE',
                        help='S-ROM (fix layer) file')
    parser.add_argument('--m1', metavar='FILE',
                        help='M-ROM (Z80 music driver) file')
    parser.add_argument('--v1', metavar='FILE',
                        help='V-ROM (ADPCM audio) file')
    parser.add_argument('--c1', metavar='FILE',
                        help='C1-ROM (sprite plane 1) file')
    parser.add_argument('--c2', metavar='FILE',
                        help='C2-ROM (sprite plane 2) file')

    # Metadata
    parser.add_argument('--name', default='ProGear Game',
                        help='Game name (max 32 chars, default: "ProGear Game")')
    parser.add_argument('--manufacturer', '--manu', default='Homebrew',
                        help='Manufacturer name (max 16 chars, default: "Homebrew")')
    parser.add_argument('--year', type=int, default=2024,
                        help='Release year (default: 2024)')
    parser.add_argument('--genre', type=int, default=0,
                        help='Genre code (default: 0)')
    parser.add_argument('--screenshot', type=int, default=0,
                        help='Screenshot index (default: 0)')
    parser.add_argument('--ngh', type=int, default=9999,
                        help='NGH number (default: 9999)')

    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Verbose output')

    args = parser.parse_args()

    # Check that at least P-ROM is provided
    if not args.p1:
        parser.error('At least --p1 (P-ROM) is required')

    if not os.path.exists(args.p1):
        print(f"Error: P-ROM file not found: {args.p1}", file=sys.stderr)
        sys.exit(1)

    try:
        build_neo_file(
            output_path=args.output,
            p1_path=args.p1,
            s1_path=args.s1,
            m1_path=args.m1,
            v1_path=args.v1,
            c1_path=args.c1,
            c2_path=args.c2,
            name=args.name,
            manufacturer=args.manufacturer,
            year=args.year,
            genre=args.genre,
            screenshot=args.screenshot,
            ngh=args.ngh,
            verbose=args.verbose
        )
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
