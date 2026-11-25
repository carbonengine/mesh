CanEdit
=======

CanEdit is a graphical editor for animation networks (``.can`` files, see :doc:`../cmf/animationNetwork`). It shows
the network as a graph of nodes, previews the result on a skinned model and lets you change the control parameters
of the network while it plays.

CanEdit is built with the other tools (``-DBUILD_TOOLS=ON``). Start it without arguments for a new network, or pass a
``.can`` file to open it:

.. code-block:: bash

   CanEdit characters/soldier.can

Window Layout
-------------

.. code-block:: text

   +------------ Menu bar (File / Edit / View / Settings) -----------+
   | Nodes      |  Root > Subgraph                 | 3D preview      |
   | (palette)  |                                  |                 |
   |------------|  Graph                           |-----------------|
   | Properties |                                  | Parameters      |
   |            |                                  |-----------------|
   |            |                                  | Problems and    |
   |            |                                  | assets          |
   +------------+----------------------------------+-----------------+
   | Time line: play / pause, step, restart, speed, time             |
   +-----------------------------------------------------------------+

The bars between the panels can be dragged to resize them. Panel sizes, the UI scale, recent files and path prefixes
are stored in the user settings (``%APPDATA%/CanEdit/settings.json`` on Windows,
``~/Library/Application Support/CanEdit/settings.json`` on macOS).

Editing Networks
----------------

Adding nodes
   Click a node type in the **Nodes** panel to add it in the middle of the graph view, or drag it onto the graph.
   Right-click empty space for the same list. Node types that the open graph can't hold are disabled: the root graph
   has a single Output node and no SubgraphInput nodes.

Connecting sockets
   Drag from an output socket to an input socket, or the other way around. Sockets and edges are colored by the type
   of data they carry: poses are orange, values green and bone weights blue. Connecting to an input that already has
   an edge replaces it. Dragging a connected input picks up its edge so that it can be moved or dropped. Dropping an
   edge on empty space opens the list of node types that fit the socket; the chosen node is added and connected.

Selecting and moving
   Click a node to select it, Ctrl+click or Shift+click to add or remove it from the selection, and drag on empty space
   to select the nodes in a rectangle. Drag selected nodes to move them. Click an edge to select it.

Properties
   The **Properties** panel edits the selected node: its name, its settings (for example the animation of an
   AnimationSample node or the text of an Expression node) and the constants of value inputs that have no edge. With
   no node selected, it edits the skeleton of the network and the preview mesh.

Subgraphs
   Double-click a Subgraph node, or select it and press Enter, to open its graph. The bar above the graph shows the
   path from the root graph; click an entry, or press Backspace, to go back. SubgraphInput and Output nodes in a
   subgraph define the sockets of the Subgraph node.

Parameters
   The **Parameters** panel lists the control parameters of the network. Add a parameter by typing its name and
   pressing Add, and drag a parameter onto the graph to create a Parameter node for it. The ``...`` button edits its
   default value and range, or removes it. Parameters used by Parameter nodes can't be removed.

Undo
   Every change can be undone and redone. Dragging nodes or changing a value counts as one step.

Copy and paste
   Copied nodes are put on the system clipboard as text, together with the edges between them, the contents of
   subgraphs and the definitions of the parameters they use. They can be pasted into another graph, another network
   or another running editor. Parameters that don't exist in the target network are added.

Keyboard and Mouse
^^^^^^^^^^^^^^^^^^

.. list-table::
    :header-rows: 1
    :widths: 35 65

    * - Input
      - Action
    * - Mouse wheel
      - Zoom the graph at the mouse position
    * - Middle drag, right drag, Space + drag
      - Pan the graph
    * - Delete
      - Delete the selected nodes or edge
    * - Ctrl+C, Ctrl+X, Ctrl+V
      - Copy, cut and paste nodes (pasted at the mouse position)
    * - Ctrl+D
      - Duplicate the selected nodes
    * - Ctrl+A
      - Select all nodes of the open graph
    * - F, Home
      - Frame the selected nodes, frame all nodes
    * - Enter, Backspace
      - Open the selected subgraph, go back to the parent graph
    * - Ctrl+Z, Ctrl+Y (Ctrl+Shift+Z)
      - Undo, redo
    * - Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Shift+S
      - New, open, save, save as

**Edit > Auto Arrange** lays out the open graph automatically. It is also applied to networks that were not saved by
CanEdit, which have no node positions.

Preview
-------

The preview plays the network on the preview meshes. A character can be made of several files, for example a body and
a head saved separately: all preview meshes are drawn together with the same pose. When no preview mesh is set, the
preview shows the skeleton file, including any meshes that file contains; if it has none, the bones are always drawn.

Edit the list under **Preview Meshes** in the **Properties** panel with no node selected: **Add Mesh...** adds a file,
and each entry can be edited, browsed for again or removed. Dropping ``.cmf`` files on the window adds them to the list.
Each mesh is bound to the network skeleton by bone names; bones that are not in the network skeleton keep their bind
pose. **Show skeleton** draws the bones on top of the meshes. A mesh that fails to load is reported in the preview and
in the **Problems and Assets** panel, and the other meshes are still shown.

Drag with the left mouse button to orbit, the right button to pan, and use the wheel or the middle button to zoom. F
frames the model.

The network is rebuilt shortly after each change. Playback continues at the same time and with the same parameter
values. The sliders in the **Parameters** panel change the parameter values of the preview only; they are not saved.

The time line plays, pauses and steps the network. Networks only play forward, so moving the time back replays the
network from the start. Length sets the range of the time slider.

Missing Assets
--------------

A network can be opened and edited while some of the files it references are missing; the problems are listed in
the **Problems and Assets** panel and marked on the nodes.

* Missing animations play the rest pose, missing bone masks weight all bones fully and unknown bones in BoneWeights
  nodes are ignored, so the rest of the network still plays.
* If the skeleton is missing, the network can't be evaluated or edited: fix the skeleton reference in the
  **Properties** panel (with no node selected) first. The preview shows the preview mesh in its rest pose.

**File > Reload Assets** reads all referenced files again, for example after they were exported again.

Path Prefixes
-------------

References to files are file paths by default. A path prefix maps a name to a directory, so that networks can use
references that don't depend on where the content is on a computer: with the prefix ``res`` mapped to
``c:\content``, the reference ``res:/characters/soldier.cmf`` resolves to ``c:\content\characters\soldier.cmf``.

Prefixes are set in **Settings > Path Prefixes**. Prefix names have at least two characters, so that they are not
mistaken for drive letters. Files picked with the Browse buttons are stored with a prefix when they are in the
directory of a prefix.

Embedded Assets
---------------

**Embed File...** in the **Problems and Assets** panel stores a CMF file in the network. References to its path then
use the embedded file, which makes the network self-contained at the cost of a larger file.

Editor Data
-----------

CanEdit stores node positions, the pan and zoom of each graph and the preview meshes in the EditorData section of the
``.can`` file (see :doc:`../cmf/animationNetworkFormat`). Other programs that load the network ignore the section.
Its contents are UTF-8 JSON:

.. code-block:: json

    {
        "format": "canedit",
        "version": 2,
        "previewMeshes": [ "res:/characters/soldier.cmf", "res:/characters/soldier_head.cmf" ],
        "graphs": [
            {
                "view": [ -120.0, -40.0, 1.0 ],
                "nodes": [ [ 600.0, 0.0 ], [ 300.0, 0.0 ], null ]
            }
        ]
    }

``previewMeshes`` lists the preview meshes. Version 1 files stored a single ``previewMesh`` string instead; CanEdit
still reads it.

``graphs`` has one entry per graph in file order (the root graph first). ``view`` is the graph position at the top
left corner of the view followed by the zoom. ``nodes`` holds the position of each node of the graph in file order, or
``null`` for nodes without a position, which are placed automatically.
