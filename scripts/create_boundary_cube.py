from pxr import Usd, UsdGeom, Sdf, Gf, Vt

OBJECT_NAME = "BoundaryCube"
CUBE_SIZE = 2.0
BOUNDARY_EDGES = (True, True, True, True)
OUTPUT_PATH = "BoundaryCube.usda"

def distance_to_edge_line(point, edge_start, edge_end):
    edge = edge_end - edge_start
    return Gf.Cross(edge, point - edge_start).GetLength() / edge.GetLength()

def create_cube(stage, path, size):
    half = size / 2.0

    points = [
        (-half, -half, -half),
        (half, -half, -half),
        (half, half, -half),
        (-half, half, -half),
        (-half, -half, half),
        (half, -half, half),
        (half, half, half),
        (-half, half, half),
    ]

    faces = [
        (0, 3, 2, 1),
        (4, 5, 6, 7),
        (0, 1, 5, 4),
        (2, 3, 7, 6),
        (1, 2, 6, 5),
        (3, 0, 4, 7),
    ]

    mesh = UsdGeom.Mesh.Define(stage, path)

    mesh.GetPointsAttr().Set(
        [Gf.Vec3f(*point) for point in points]
    )

    mesh.GetFaceVertexCountsAttr().Set(
        [4] * len(faces)
    )

    mesh.GetFaceVertexIndicesAttr().Set(
        [index for face in faces for index in face]
    )

    mesh.GetSubdivisionSchemeAttr().Set("none")

    distances = [0.0] * (len(points) * 4)

    for polygon in faces:
        corners = [
            Gf.Vec3f(*points[index])
            for index in polygon
        ]

        for corner, vertex_index in enumerate(polygon):
            channels = [0.0] * 4

            for edge in range(4):
                if not BOUNDARY_EDGES[edge]:
                    continue

                edge_start = corners[edge]
                edge_end = corners[(edge + 1) % 4]

                channels[edge] = distance_to_edge_line(
                    corners[corner],
                    edge_start,
                    edge_end,
                )

            start = vertex_index * 4

            for channel in range(4):
                distances[start + channel] = channels[channel]

    primvars = UsdGeom.PrimvarsAPI(mesh)

    boundary_distances = primvars.CreatePrimvar(
        "boundaryDistances",
        Sdf.ValueTypeNames.FloatArray,
        UsdGeom.Tokens.vertex,
        elementSize=4,
    )

    boundary_distances.Set(
        Vt.FloatArray(distances)
    )

    return mesh

stage = Usd.Stage.CreateNew(OUTPUT_PATH)

create_cube(
stage,
f"/{OBJECT_NAME}",
CUBE_SIZE,
)

stage.GetRootLayer().Save()

print(f"Created {OUTPUT_PATH}")

