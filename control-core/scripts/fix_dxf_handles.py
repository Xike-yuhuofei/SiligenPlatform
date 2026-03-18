#!/usr/bin/env python3
"""Fix DXF files missing entity handles (group code 5)."""

import sys
import re


def fix_dxf_handles(input_path: str, output_path: str) -> None:
    """Add missing handles to DXF entities."""
    with open(input_path, 'r', encoding='utf-8', errors='replace') as f:
        lines = f.read().splitlines()

    # Entity types that need handles
    entity_types = {
        'LWPOLYLINE', 'POLYLINE', 'LINE', 'ARC', 'CIRCLE', 'POINT',
        'VERTEX', 'SEQEND', 'SPLINE', 'ELLIPSE', 'INSERT', 'TEXT',
        'MTEXT', 'DIMENSION', 'LEADER', 'HATCH', 'SOLID', '3DFACE'
    }

    handle_counter = 0x2000  # Start from 0x2000 to avoid conflicts
    output_lines = []
    i = 0

    while i < len(lines):
        line = lines[i].strip()
        output_lines.append(lines[i])

        # Check if this is an entity type declaration (group code 0)
        if line == '0' and i + 1 < len(lines):
            next_line = lines[i + 1].strip()
            if next_line in entity_types:
                # Add entity type
                i += 1
                output_lines.append(lines[i])

                # Check if next group code is 5 (handle)
                if i + 1 < len(lines) and lines[i + 1].strip() != '5':
                    # Missing handle, add one
                    output_lines.append('5')
                    output_lines.append(f'{handle_counter:X}')
                    handle_counter += 1

        i += 1

    with open(output_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(output_lines))

    print(f"Fixed DXF saved to: {output_path}")
    print(f"Added {handle_counter - 0x2000} handles")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.dxf> <output.dxf>")
        sys.exit(1)

    fix_dxf_handles(sys.argv[1], sys.argv[2])
