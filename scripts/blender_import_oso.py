import glob
import os

import bpy
import oslquery
from cycles.osl import update_script_node

OSO_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..",
    "build",
    "oso",
)

def output_closure_name(oso_path):
    q = oslquery.OSLQuery(oso_path)
    if not q:
        return None
    for param in q.parameters:
        if param.isoutput and param.isclosure:
            return param.name
    return None


def _report(level, msg):
    print(f"  [{'/'.join(level)}] {msg}")


def import_oso_material(oso_path: str) -> bpy.types.Material:
    mat_name = os.path.splitext(os.path.basename(oso_path))[0]
    abs_path = os.path.abspath(oso_path)

    # Reuse or create the material
    mat = bpy.data.materials.get(mat_name)
    if mat is None:
        mat = bpy.data.materials.new(name=mat_name)

    # Fake user keeps the material alive even when no objects reference it
    mat.use_fake_user = True
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    nodes.clear()

    output_node = nodes.new("ShaderNodeOutputMaterial")
    output_node.location = (400, 0)

    script_node = nodes.new("ShaderNodeScript")
    script_node.mode = "EXTERNAL"
    script_node.filepath = abs_path
    script_node.location = (0, 0)
    script_node.update()

    # update_script_node — it calls oslquery internally and
    # calls shader_param_ensure() to create every input/output socket on the
    # node, exactly the same as when a user loads a script in the UI.
    update_script_node(script_node, _report)

    # 'out' by convention. fall back to oslquery if the name differs.
    socket_name = "out"
    if socket_name not in script_node.outputs:
        socket_name = output_closure_name(abs_path) or socket_name

    out_socket = script_node.outputs.get(socket_name)
    if out_socket is None:
        print(f"  WARNING: could not resolve output closure socket for {mat_name} — "
              f"material created without Surface link")
        return mat

    links.new(out_socket, output_node.inputs["Surface"])

    print(f"  Imported: {mat_name}  ({abs_path})")
    return mat


def main():
    bpy.context.scene.render.engine = "CYCLES"

    oso_pattern = os.path.join(OSO_DIR, "*.oso")
    oso_files = sorted(glob.glob(oso_pattern))

    if not oso_files:
        print(f"No .oso files found in: {os.path.abspath(OSO_DIR)}")
        return

    print(f"Found {len(oso_files)} OSO file(s) in {os.path.abspath(OSO_DIR)}\n")

    for oso_path in oso_files:
        import_oso_material(oso_path)

    print(f"\nDone — {len(oso_files)} material(s) imported into the current .blend.")

    bpy.ops.wm.save_mainfile()
    print(f"Saved: {bpy.data.filepath}")


if __name__ == "__main__":
    main()
