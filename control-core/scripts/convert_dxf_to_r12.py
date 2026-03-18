#!/usr/bin/env python3
"""Convert DXF to R12 format using ezdxf for maximum compatibility."""

import sys
try:
    import ezdxf
    from ezdxf.addons import odafc
except ImportError:
    print("Error: ezdxf not installed. Run: pip install ezdxf")
    sys.exit(1)


def convert_to_r12(input_path: str, output_path: str) -> None:
    """Convert any DXF to R12 format for maximum compatibility."""
    print(f"Reading: {input_path}")

    try:
        doc = ezdxf.readfile(input_path)
    except Exception as e:
        print(f"Error reading file: {e}")
        print("Trying recovery mode...")
        try:
            doc, auditor = ezdxf.recover.readfile(input_path)
            if auditor.has_errors:
                print(f"Recovery found {len(auditor.errors)} errors")
                for error in auditor.errors[:10]:
                    print(f"  - {error}")
        except Exception as e2:
            print(f"Recovery also failed: {e2}")
            sys.exit(1)

    print(f"Original version: {doc.dxfversion}")
    print(f"Entities count: {len(doc.modelspace())}")

    # Create new R12 document
    new_doc = ezdxf.new('R12')
    msp = new_doc.modelspace()

    # Copy entities
    entity_count = 0
    for entity in doc.modelspace():
        try:
            dxftype = entity.dxftype()

            if dxftype == 'LINE':
                msp.add_line(
                    start=(entity.dxf.start.x, entity.dxf.start.y),
                    end=(entity.dxf.end.x, entity.dxf.end.y),
                    dxfattribs={'layer': entity.dxf.layer}
                )
                entity_count += 1

            elif dxftype == 'LWPOLYLINE':
                points = list(entity.get_points(format='xy'))
                if entity.closed:
                    msp.add_polyline2d(points, close=True, dxfattribs={'layer': entity.dxf.layer})
                else:
                    msp.add_polyline2d(points, close=False, dxfattribs={'layer': entity.dxf.layer})
                entity_count += 1

            elif dxftype == 'POLYLINE':
                points = [(v.dxf.location.x, v.dxf.location.y) for v in entity.vertices]
                msp.add_polyline2d(points, close=entity.is_closed, dxfattribs={'layer': entity.dxf.layer})
                entity_count += 1

            elif dxftype == 'CIRCLE':
                msp.add_circle(
                    center=(entity.dxf.center.x, entity.dxf.center.y),
                    radius=entity.dxf.radius,
                    dxfattribs={'layer': entity.dxf.layer}
                )
                entity_count += 1

            elif dxftype == 'ARC':
                msp.add_arc(
                    center=(entity.dxf.center.x, entity.dxf.center.y),
                    radius=entity.dxf.radius,
                    start_angle=entity.dxf.start_angle,
                    end_angle=entity.dxf.end_angle,
                    dxfattribs={'layer': entity.dxf.layer}
                )
                entity_count += 1

            elif dxftype == 'POINT':
                msp.add_point(
                    location=(entity.dxf.location.x, entity.dxf.location.y),
                    dxfattribs={'layer': entity.dxf.layer}
                )
                entity_count += 1

            elif dxftype == 'SPLINE':
                # Convert spline to polyline approximation
                try:
                    path = ezdxf.path.make_path(entity)
                    points = list(path.flattening(0.1))
                    if len(points) >= 2:
                        xy_points = [(p.x, p.y) for p in points]
                        msp.add_polyline2d(xy_points, dxfattribs={'layer': entity.dxf.layer})
                        entity_count += 1
                except:
                    print(f"  Warning: Could not convert SPLINE")

            else:
                print(f"  Skipping unsupported entity type: {dxftype}")

        except Exception as e:
            print(f"  Error copying entity {entity.dxftype()}: {e}")

    print(f"Converted {entity_count} entities")

    # Save as R12
    new_doc.saveas(output_path)
    print(f"Saved R12 format to: {output_path}")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.dxf> <output.dxf>")
        sys.exit(1)

    convert_to_r12(sys.argv[1], sys.argv[2])
