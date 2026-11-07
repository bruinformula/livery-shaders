"""Build the RenderMan for Houdini nodes of the compiled shaders into one HDA. Run with hython

RfH can do this itself at every Houdini start (RFH_ARGS2HDA=1), but it then converts every .oso on
RMAN_SHADERPATH each launch.

    hython scripts/build_houdini_hda.py <output.hda> <dir with .oso files>...
"""
import os
import sys

import hou
import rfh.convertutils
import rfh.rmands
from rfh.rfh_node_desc import RfhNodeDesc
from rman_utils.filepath import FilePath


def tab_menus(shader_dirs):
    """Node type name -> Tab menu submenu, from each shader's rfh_classification."""
    menus = {}
    for d in shader_dirs:
        for f in os.listdir(d):
            if f.endswith(".oso"):
                try:
                    menu = RfhNodeDesc(FilePath(os.path.join(d, f))).rfh_classification
                except Exception:
                    continue
                if menu:
                    menus[f[:-4].lower()] = menu
    return menus


def main():
    output, shader_dirs = sys.argv[1], sys.argv[2:]
    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
    if os.path.exists(output):
        os.remove(output)
    rfh.convertutils.main(shader_dirs, otlfile=output)

    # Merging many shaders into one HDA drops RfH's Tab menu tools: add them back
    menus = tab_menus(shader_dirs)
    hou.hda.installFile(output)
    for definition in hou.hda.definitionsInFile(output):
        menu = menus.get(definition.nodeTypeName())
        if menu:
            definition.addSection("Tools.shelf", rfh.rmands.TOOL_TEMPLATE % (menu, "voptoolutils", "voptoolutils"))
    print(f"Wrote {output}")


main()
