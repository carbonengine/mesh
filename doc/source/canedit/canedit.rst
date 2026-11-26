CanEdit
=======

CanEdit is a graphical editor for animation networks (see :doc:`../cmf/animationNetwork`). It shows the network as a
graph of nodes, previews the result on a skinned model and lets you change the control parameters of the network while
it plays.

CanEdit saves networks as documents (``.cane`` files, see `Documents and Export`_) and exports them as ``.can`` files,
which the runtime loads.

CanEdit is built with the other tools (``-DBUILD_TOOLS=ON``). Start it without arguments for a new network, or pass a
``.cane`` document to open it:

.. code-block:: bash

   CanEdit characters/soldier.cane

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
   and the graphs of states have a single Output node and no SubgraphInput nodes.

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
   AnimationSample node or the text of an Expression node) and the constants of value inputs that have no edge.
   Boolean inputs such as ``loop`` are shown as checkboxes and Integer inputs such as ``selector`` as whole numbers.
   With no node selected, it edits the skeleton of the network and the preview meshes.

Subgraphs
   Double-click a Subgraph node, or select it and press Enter, to open its graph. The bar above the graph shows the
   path from the root graph; click an entry, or press Backspace, to go back. SubgraphInput and Output nodes in a
   subgraph define the sockets of the Subgraph node.

State machines
   Double-click a StateMachine node, or select it and press Enter, to open the state machine view: states are drawn as
   boxes and transitions as arrows. See `Editing State Machines`_.

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

Editing State Machines
^^^^^^^^^^^^^^^^^^^^^^

A StateMachine node plays one of its states at a time and switches between them along transitions (see
:doc:`../cmf/animationNetwork` for how they behave). The state machine view, opened from the node, shows its states
and transitions:

* Right-click empty space and choose **Add State** to add a state. A new state has an Output node, waiting for the
  pose of the state to be connected.
* Double-click a state, or select it and press Enter, to open its graph and edit it like any other graph. The bar
  above the graph shows the state machine and the state; Backspace goes back to the state machine view.
* Drag from the edge of a state onto another state to add a transition; the mouse cursor changes to a hand on the
  edge. A transition from a state back to itself is added with **Add Transition To** in the context menu of the state.
* Click a state or a transition to select it, and drag states to move them. Delete removes the selected states, with
  their transitions, or the selected transition.

The start state has a green border and an arrow pointing at it; set it with **Set as Start State** in the context menu
of a state or in the **Properties** panel. While the preview plays, the current state is highlighted.

With a state selected, the **Properties** panel edits its name and lists its transitions. With a transition selected,
it edits the condition: an expression over the parameters and ``stateTime``, the end of the animation of the state, or
a timeout. It also edits the blend duration. Transitions with problems, such as an expression that doesn't parse, are
drawn red and explained in the panel. With nothing selected, the panel edits the node: its name, its start state and
whether it resumes its previous state when it becomes active again.

Copying a StateMachine node copies its states, their graphs and its transitions, together with the parameters its
transition expressions use. States themselves can't be copied.

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
      - Delete the selected nodes or edge, or the selected states or transition
    * - Ctrl+C, Ctrl+X, Ctrl+V
      - Copy, cut and paste nodes (pasted at the mouse position)
    * - Ctrl+D
      - Duplicate the selected nodes
    * - Ctrl+A
      - Select all nodes of the open graph, or all states of the open state machine
    * - F, Home
      - Frame the selected nodes, frame all nodes
    * - Enter, Backspace
      - Open the selected subgraph, state machine or state, go back
    * - Ctrl+Z, Ctrl+Y (Ctrl+Shift+Z)
      - Undo, redo
    * - Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Shift+S
      - New, open, save, save as
    * - Ctrl+E, Ctrl+Shift+E
      - Export, export as

**Edit > Auto Arrange** lays out the open graph automatically, or arranges the states of the open state machine in a
grid. It is also applied to nodes without a position when a
document is opened, for example in documents written by other tools.

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

An embedded asset is a CMF file whose contents are packed into the exported ``.can`` file. References to its path
then use the embedded copy instead of loading the file, which makes the exported network self-contained at the cost
of a larger file. Only the skeleton and the animations the network uses from the file are exported; meshes and
unused skeletons and animations are left out.

**Embed File...** in the **Problems and Assets** panel marks a file as embedded. The document only stores its
reference: the file is read when the document is opened, when **File > Reload Assets** is used and again when the
network is exported, so the exported network always contains the current file. Undo and redo reuse the contents read
before.

An embedded file that can't be read is listed with the error in the panel and the network uses the asset loader for
its path, as if it weren't embedded. A network whose embedded files can't be read is not exported, and neither is a
network that uses a skeleton or animation an embedded file doesn't contain.

Documents and Export
--------------------

**File > Save** writes the document: a UTF-8 JSON file with the ``.cane`` extension that holds the whole network
together with the node positions, the pan and zoom of each graph, the preview meshes and the export path. Documents are
meant to be kept in source control: they are indented, and nodes are stored in a stable order.

**File > Export** (Ctrl+E) writes the network in the CAN format to the export path of the document. The first export
asks for the path, and **Export As...** (Ctrl+Shift+E) always asks. The export path is stored in the document, relative
to the document file, so changing it is a change of the document. Exporting doesn't save the document.

Networks with errors (see the **Problems and Assets** panel) are not exported. Assets that are still loading and
warnings, such as missing external animations, don't prevent the export. Animations missing from embedded files do,
see `Embedded Assets`_.

Exported ``.can`` files can't be opened in CanEdit: they only contain what the runtime needs. Keep the ``.cane``
document and export it again after changes.

Document Format
^^^^^^^^^^^^^^^

An abbreviated document:

.. code-block:: json

    {
        "format": "cane",
        "version": 1,
        "skeleton": { "asset": "res:/characters/soldier.cmf", "name": "" },
        "parameters": [ { "name": "speed", "default": 0.0, "min": 0.0, "max": null } ],
        "embeddedAssets": [ "res:/characters/soldier_idle.cmf" ],
        "previewMeshes": [ "res:/characters/soldier.cmf", "res:/characters/soldier_head.cmf" ],
        "exportPath": "../export/soldier.can",
        "graph": {
            "view": [ -120.0, -40.0, 1.0 ],
            "nodes": [
                {
                    "type": "Output", "name": "", "position": [ 600.0, 0.0 ], "inputs": [ 0.0 ],
                    "settings": { "socketName": "", "socketType": "Pose" }
                },
                {
                    "type": "AnimationSample", "name": "Idle", "position": [ 300.0, 0.0 ], "inputs": [ 0.0, 1.0, 1.0 ],
                    "settings": { "asset": "res:/characters/soldier_idle.cmf", "animation": "Idle" }
                }
            ],
            "edges": [ [ 1, 0, 0, 0 ] ]
        }
    }

* ``format`` and ``version`` identify the file. CanEdit rejects documents of newer versions.
* ``parameters`` are the control parameters in order. Limits without a value (``null``) are unbounded.
* ``graph`` is the root graph. ``view`` is the graph position at the top left corner of the view followed by the
  zoom. ``nodes`` lists the nodes with their type, name, position, input constants (one per input; ``null`` for no
  value) and type specific settings; Parameter nodes reference parameters by name. Subgraph nodes contain their graph
  in ``graph``, in the same form. ``edges`` connects ``[ output node, output socket, input node, input socket ]``,
  with nodes referenced by their index in ``nodes``.

Nodes without a position are placed automatically. Parts of a document that can't be applied, for example an edge
between sockets of different types or an unknown node type, are skipped with a warning when the document is opened;
the document is then marked as modified.
