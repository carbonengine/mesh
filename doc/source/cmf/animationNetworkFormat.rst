CAN File Format
===============

CAN (Carbon Animation Network) files store animation networks (see :doc:`animationNetwork`). The structures are
declared in ``cmf/can.h``, namespace ``cmf::can::v1``.

The format uses the same approach as CMF (see :doc:`fileFormat`): structures are stored as-is, and ``cmf::Span``
members are stored as offsets relative to the span itself, so the data can be used directly from the loaded file
buffer without fix-ups. All structures are free of implicit padding; reserved fields are always zero.

File Structure
--------------

A CAN file consists of:

#. A ``Header`` with the section table.
#. The Network section, holding a ``NetworkData`` structure. It is the only section.

The section starts at an offset aligned to 8 bytes.

Header
^^^^^^

.. list-table::
    :header-rows: 1
    :widths: 20 20 60

    * - Field
      - Type
      - Description
    * - ``signature``
      - ``uint32_t``
      - ``0x666e6163``, the bytes ``canf``
    * - ``version``
      - ``uint32_t``
      - Format version, currently 1
    * - ``headerSize``
      - ``uint32_t``
      - Size of the header including the section table
    * - ``crc32``
      - ``uint32_t``
      - CRC32 of the rest of the file, starting after this field
    * - ``sections``
      - ``Span<Section>``
      - Section table with a single entry: offset, size and type (``Network``) of the Network section

Network Data
------------

``NetworkData`` is the root structure of the Network section:

* ``assets``: the external CMF files the network references, loaded through the asset loader. Each asset has a path;
  each path appears once.
* ``skeletonAsset`` and ``skeletonName``: the external asset containing the skeleton and its name; an empty name
  selects the first skeleton. ``skeletonAsset`` is ``NO_INDEX`` (``0xffffffff``) if the skeleton is embedded.
* ``skeleton``: the embedded skeleton, a ``cmf::Skeleton``, or empty if the skeleton is external. For an embedded
  skeleton, ``skeletonName`` keeps the name it was referenced by.
* ``animations``: the embedded animations, ``cmf::Animation`` structures referenced by AnimationSample nodes.
* ``parameters``: the control parameters, with name, default value and range.
* ``graphs``: the graphs of the network. ``graphs[0]`` is the root graph; the others are subgraphs, each referenced
  by exactly one Subgraph node.

Embedded Skeletons and Animations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Embedded skeletons and animations use the CMF structures (see :doc:`fileFormat`), stored in the Network section like
the rest of the network. Only what the network uses is stored: the skeleton of the network and the animations that
AnimationSample nodes sample, each animation once. Meshes and other skeletons and animations of the embedded CMF
files are left out. References to embedded data are resolved when the network is saved, so embedded data is
referenced by index rather than by name.

Editor Data
-----------

CAN files only hold what the runtime needs. Editing tools keep their own data, such as node positions, in their own
files and export CAN files from them; CanEdit, for example, saves ``.cane`` documents (see :doc:`../canedit/canedit`).
Tools that associate data with nodes of a saved file refer to nodes by their position in the file: graph index and
node index. The order in which ``AnimationNetworkBuilder::Save`` stores graphs and nodes is returned by
``GetSaveLayout``.

Graphs
^^^^^^

A ``Graph`` contains:

* ``nodes``: the nodes. A ``Node`` stores what all node types have in common: the node type, an optional name, the
  constant values of its inputs (one per input socket, used by unconnected Float inputs) and ``dataIndex``.
* ``edges``: connections from an output socket to an input socket. Sockets are referenced by node index and socket
  index; the sockets of each node type are listed in the node reference of :doc:`animationNetwork`.
* ``interfaceInputs`` and ``interfaceOutputs``: the indices of the SubgraphInput and Output nodes, in the order of the
  sockets they define on the Subgraph node. The root graph has no SubgraphInput nodes and at most one Output node.
* One table per node type with type specific data. ``Node::dataIndex`` is the index of the node data in the table
  of its type. RestPose, Blend and AdditiveBlend nodes have no data and use ``NO_INDEX``.

.. list-table::
    :header-rows: 1
    :widths: 30 30 40

    * - Node types
      - Table
      - Data
    * - SubgraphInput, Output
      - ``interfaceNodes``
      - Socket type and name
    * - Parameter
      - ``parameterNodes``
      - Parameter index
    * - AnimationSample
      - ``animationSampleNodes``
      - External asset index or embedded animation index, animation name
    * - Switch
      - ``switchNodes``
      - Number of pose inputs
    * - RandomSwitch
      - ``randomSwitchNodes``
      - Number of pose inputs, flags, input weights
    * - NamedBoneWeights
      - ``namedBoneWeightsNodes``
      - Bone mask name
    * - BoneWeights
      - ``boneWeightsNodes``
      - Default weight, weights of bones by name
    * - Subgraph
      - ``subgraphNodes``
      - Index of the graph in ``NetworkData::graphs``
    * - Expression
      - ``expressionNodes``
      - Expression text, input names

Expressions are stored as text and compiled when the network is loaded, so files don't depend on the bytecode of
the expression parser.

Identical arrays are stored once: when the file is written, spans with identical contents share their data.
Programs that modify a CAN file in place must take into account that a change can affect several spans.

Validation
----------

``cmf::AnimationNetwork::Load`` rejects files that are not well formed. It checks:

* The signature, version, header size and CRC.
* That the file has a single section, the Network section, and that it lies within the file.
* That all spans are aligned and lie within the Network section, and that the embedded skeleton and animations are
  valid.
* That all indices are in range: assets, embedded animations, parameters, graphs and node data. Each node data entry
  belongs to one node. The skeleton and each AnimationSample node are either external or embedded, not both.
* That node and socket types are valid, that every node has one constant per input socket and that Switch nodes
  have pose inputs.
* That subgraphs form a tree below the root graph.
* That edges connect existing sockets of the same type, that every input has at most one incoming edge and that
  the edges of each graph form no cycle.

Well-formed files may still describe incomplete networks, for example with unconnected pose inputs. Such networks
load, but are not ready for evaluation; ``GetErrors`` describes the problems. Problems the network works around,
such as missing animations, are described by ``GetWarnings``.

Versioning
----------

Adding fields or node data tables changes the size of the structures and therefore requires a new ``FILE_VERSION``.
Adding a node type without data doesn't change the structures, but files using it are rejected by older versions of
the library, which don't know the node type.
