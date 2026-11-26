Animation Networks
==================

An animation network is a graph of nodes that combines skeleton poses into one output pose. Nodes sample
animations, blend and switch between poses and compute blend factors from control parameters. Edges between nodes
show the data flow. A game sets the control parameters, for example the movement speed of a character, and the
network turns them into a pose every frame.

Networks are stored in CAN files (Carbon Animation Network, ``.can``), see :doc:`animationNetworkFormat`.

Overview
--------

The feature consists of three classes:

* ``cmf::AnimationNetworkBuilder`` (``cmf/animationnetworkbuilder.h``) creates and edits networks. It is meant for
  tools: it uses simple, editable data structures, validates the network while it is being edited and saves it as a
  CAN file.
* ``cmf::AnimationNetwork`` (``cmf/animationnetwork.h``) is a read-only network loaded from a CAN file. It reads the
  network directly from the file buffer and loads the CMF files the network references. Games create one network
  per network asset.
* ``cmf::AnimationNetworkInstance`` (``cmf/animationnetworkinstance.h``) evaluates a network for one animated
  object. It holds the playback state, such as the time of every animation, and computes the pose. Games create one
  instance per animated object.

.. code-block:: text

    Tool:   AnimationNetworkBuilder --Save()--> .can file
    Game:   .can file --Load()--> AnimationNetwork --> AnimationNetworkInstance (one per object)

Concepts
--------

Graphs, Nodes and Sockets
^^^^^^^^^^^^^^^^^^^^^^^^^

A network has a root graph. A graph contains nodes and edges. Each node has input and output sockets; an edge
connects an output socket of one node to an input socket of another node. Sockets have one of three types, and
edges can only connect sockets of the same type:

* **Pose**: one local transform per skeleton bone.
* **BoneWeights**: one weight per skeleton bone, used to restrict blends to parts of the skeleton.
* **Float**: a single number, used for blend factors, speeds, selectors and so on.

Graphs must be acyclic. The root graph has an Output node whose input is the result of the network.

Inputs that are not connected behave as follows:

* Pose inputs must be connected; a network with an unconnected pose input can't be evaluated.
* Float inputs use a constant value stored in the node. Every node type has sensible defaults, for example a speed
  of 1. Editors show some Float inputs differently: ``loop`` as a Boolean and ``selector`` as an Integer (see
  `Value Kinds`_).
* BoneWeights inputs are optional; without them, a blend applies to all bones in the same way.

Only nodes connected to the result count. A node is **used** if it is the Output node of the root graph, the Output
node of a state of a used StateMachine node, a SubgraphInput or Output node in the graph of a used Subgraph node, or
if one of its outputs is connected to a used node. ``AnimationNetworkBuilder`` saves only used nodes and doesn't
report problems of the others. Nodes that are still being wired up don't stop the rest of the network from being
saved and evaluated.

Control Parameters
^^^^^^^^^^^^^^^^^^

A network defines named control parameters, each with a default value and a range. Parameter nodes output the
current value of a parameter, and several Parameter nodes may output the same parameter. Instances clamp the values
they are given to the parameter range.

Expressions
^^^^^^^^^^^

Expression nodes compute a Float from Float inputs with an expression such as ``min( speed / 4, 1 )``. Every input
name of the expression becomes an input socket of the node. Expressions are parsed by the
`carbonengine parser <https://github.com/carbonengine/parser>`_ library. They support arithmetic, comparisons,
logical operators, the ternary operator and functions such as ``min``, ``max``, ``abs``, ``sqrt``, ``sin`` and
``cos``.

Subgraphs
^^^^^^^^^

A Subgraph node contains a graph of its own. SubgraphInput nodes inside the graph define the input sockets of the
Subgraph node, and Output nodes define its output sockets. Each of them declares the name and type of its socket.
Subgraphs group parts of a network; they use the skeleton and control parameters of the network and have no
runtime cost, because they are inlined when the network is prepared for evaluation.

State Machines
^^^^^^^^^^^^^^

A StateMachine node plays one of its states at a time. Each state is a graph of its own, like the root graph: it has
one Pose Output node and no SubgraphInput nodes, and it reads values through Parameter nodes. Directed transitions
connect the states, and the output of the node is the pose of the current state.

One state is the start state. The machine enters it the first time the node contributes to the pose. When the node
contributes to the pose again after it didn't (for example after the alpha of a Blend above it was 0), it restarts
from the start state, or, with **resume on activate**, continues in the state it was in.

Each transition has a condition. Every time the network is evaluated, the conditions of the transitions leaving the
current state are checked, and the first one that holds is taken; at most one transition is taken per evaluation.
The conditions are:

* **Expression**: an expression like the ones of Expression nodes, true when it is not 0. It uses the control
  parameters by name and ``stateTime``, the time in seconds spent in the current state, for example
  ``speed > 2 || stateTime > 5``. A parameter named ``stateTime`` hides the built-in variable.
* **Animation finished**: true when the animation of the current state reaches its end. Only states whose Output node
  is connected directly to an AnimationSample node (possibly through subgraphs) with an unconnected ``loop`` input of
  0 and an unconnected ``time`` input finish; other nodes may drive its ``speed``. All other states, for example
  looping or blended animations, never finish.
* **Timeout**: true when the machine has spent the timeout in the current state.

Entering a state restarts the playheads of all AnimationSample nodes in its graph, including nested subgraphs and
state machines, so that the state plays its animations from the start.

A transition with a duration blends from the previous pose to the new state over that time: the new state fades in
on top of the pose so far. If another transition is taken while a state is still fading in, the new state fades in
on top of that blend, so several states can fade out at the same time. A state that is fully faded in hides the ones
below it, which then stop being evaluated. At most eight states are blended; the oldest one is dropped if a ninth
transition starts.

Assets
^^^^^^

The network references a CMF file with the skeleton it animates, and AnimationSample nodes reference CMF files with
animations. Files are referenced by asset path. Paths are resolved by an asset loader that the application provides
(see `The Asset Loader`_), unless a CMF file with the same path is embedded in the network (see `Embedding Assets`_).

Animation channels are matched to the bones of the network skeleton by bone name, so animations don't have to use
the same bone order or contain all bones. Bones that an animation doesn't animate keep their rest pose.

Node Reference
--------------

.. list-table::
    :header-rows: 1
    :widths: 18 40 12 30

    * - Node
      - Inputs
      - Output
      - Settings
    * - Output
      - pose (Pose)
      - \-
      - In subgraphs: socket name and type
    * - SubgraphInput
      - \-
      - declared type
      - Socket name and type (subgraphs only)
    * - Parameter
      - \-
      - Float
      - Network parameter
    * - AnimationSample
      - time, loop (Boolean, default 1), speed (default 1) (Float)
      - Pose
      - Asset path, animation name
    * - RestPose
      - \-
      - Pose
      - \-
    * - Blend
      - poseA, poseB (Pose), alpha (Float), weights (BoneWeights)
      - Pose
      - \-
    * - Switch
      - selector (Float, Integer), pose0 … poseN-1 (Pose)
      - Pose
      - Number of pose inputs
    * - RandomSwitch
      - pose0 … poseN-1 (Pose)
      - Pose
      - Number of pose inputs, input weights, reroll on activation
    * - AdditiveBlend
      - pose, base, additive (Pose), alpha (Float, default 1), weights (BoneWeights)
      - Pose
      - \-
    * - NamedBoneWeights
      - \-
      - BoneWeights
      - Name of a bone mask of the skeleton
    * - BoneWeights
      - \-
      - BoneWeights
      - Weights of individual bones by name, weight of other bones
    * - Subgraph
      - One per SubgraphInput node of its graph
      - One per Output node of its graph
      - The contained graph
    * - Expression
      - One Float per input name
      - Float
      - Expression text, input names
    * - StateMachine
      - \-
      - Pose
      - States (each a graph), transitions, start state, resume on activate

The nodes behave as follows:

* **AnimationSample** samples an animation. Each AnimationSample node has a playhead that advances by the elapsed
  time multiplied by the ``speed`` input. With ``loop`` set to a non-zero value the animation repeats, otherwise it
  stops at its end. When the ``time`` input is connected, the node samples the animation at that time instead of
  using its playhead.
* **Blend** interpolates between ``poseA`` (alpha 0) and ``poseB`` (alpha 1), see ``cmf::BlendPoses``. Alpha is
  clamped to [0, 1]. With bone weights, the alpha of each bone is multiplied by its weight.
* **AdditiveBlend** adds the difference between ``additive`` and ``base`` to ``pose``, scaled by alpha, see
  ``cmf::BlendAdditivePose``.
* **Switch** outputs one of its pose inputs. The selector is truncated: values in [0, 1) select the first input,
  [1, 2) the second one and so on. Values outside the range select the first or last input.
* **RandomSwitch** outputs one of its pose inputs chosen at random, with probabilities proportional to the input
  weights (uniform if no weights are set). It picks a new input when the selected input finishes an animation cycle
  (loops or reaches its end) and, if enabled, when the node becomes active again after not contributing to the pose.
* **NamedBoneWeights** and **BoneWeights** provide bone weights for blends.
* **StateMachine** outputs the pose of its current state, blended with fading states, see `State Machines`_.

Building Networks
-----------------

``cmf::AnimationNetworkBuilder`` holds the root graph (``GetRootGraph``), the control parameters, the skeleton
reference and embedded assets. Graphs are edited with ``cmf::AnimationGraphBuilder``: nodes are added with
``AddNode``, configured with type specific setters and connected with ``Connect``. Nodes are referenced by
``cmf::AnimationNodeId`` handles, sockets by a node handle and a socket index.

The following network blends between an idle and a walk animation based on a ``speed`` parameter:

.. code-block:: cpp

    #include <cmf/animationnetworkbuilder.h>

    cmf::AnimationNetworkBuilder builder( loader );
    builder.SetSkeleton( "characters/soldier.cmf", "" );

    cmf::AnimationParameter speed;
    speed.name = "speed";
    speed.minValue = 0.f;
    speed.maxValue = 4.f;
    const uint32_t speedIndex = builder.AddParameter( speed );

    auto& graph = builder.GetRootGraph();
    const auto output = graph.AddNode( cmf::AnimationNodeType::Output );
    const auto idle = graph.AddNode( cmf::AnimationNodeType::AnimationSample );
    const auto walk = graph.AddNode( cmf::AnimationNodeType::AnimationSample );
    const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
    const auto parameter = graph.AddNode( cmf::AnimationNodeType::Parameter );
    const auto alpha = graph.AddNode( cmf::AnimationNodeType::Expression );

    graph.SetAnimation( idle, "characters/soldier_idle.cmf", "idle" );
    graph.SetAnimation( walk, "characters/soldier_walk.cmf", "walk" );
    graph.SetParameter( parameter, speedIndex );
    graph.SetExpression( alpha, "min( speed, 1 )", { "speed" } );

    graph.Connect( { parameter, 0 }, { alpha, 0 } );
    graph.Connect( { idle, 0 }, { blend, 0 } );
    graph.Connect( { walk, 0 }, { blend, 1 } );
    graph.Connect( { alpha, 0 }, { blend, 2 } );
    graph.Connect( { blend, 0 }, { output, 0 } );

    if( !builder.IsReady() )
    {
        for( const auto& error : builder.GetErrors() )
        {
            // Report the problem, for example an unconnected input
        }
    }
    for( const auto& warning : builder.GetWarnings() )
    {
        // Report problems that don't prevent evaluation, for example a missing animation
    }
    std::vector<uint8_t> file;
    const auto saved = builder.Save( file );
    if( !saved )
    {
        // saved.error describes the problem, for example an animation missing from an embedded file
    }

Editing operations return a ``cmf::ValidationResult`` describing why an operation failed, for example a connection
between sockets of different types or a connection that would create a cycle. ``GetErrors`` lists problems that
prevent the network from being evaluated, such as unconnected pose inputs. ``GetWarnings`` lists problems that the
network works around, such as animations missing from their files. Both only check used nodes (see
`Graphs, Nodes and Sockets`_): a Blend node that isn't connected to anything yet is not an error. Networks with
errors can still be saved; ``cmf::AnimationNetwork`` reports the errors when it loads them. Saving only fails if a
skeleton or animation referenced from an embedded file is missing (see `Embedding Assets`_).

The type specific settings of a node are returned by ``GetSettings``, for example
``graph.GetSettings<cmf::AnimationSampleSettings>( idle )``.

Building State Machines
^^^^^^^^^^^^^^^^^^^^^^^

States are added to a StateMachine node with ``AddState``, which returns the index of the state. ``GetStateGraph``
returns the graph of a state; it is edited like any other graph. Transitions are added with ``AddTransition``:

.. code-block:: cpp

    const auto machine = graph.AddNode( cmf::AnimationNodeType::StateMachine );
    graph.Connect( { machine, 0 }, { output, 0 } );

    // Adds a state that plays an animation
    auto addState = [&]( const char* name, const char* animation, bool loop ) {
        const uint32_t state = graph.AddState( machine, name );
        auto& stateGraph = *graph.GetStateGraph( machine, state );
        const auto stateOutput = stateGraph.AddNode( cmf::AnimationNodeType::Output );
        const auto sample = stateGraph.AddNode( cmf::AnimationNodeType::AnimationSample );
        stateGraph.SetAnimation( sample, "characters/soldier_anims.cmf", animation );
        stateGraph.SetInputConstant( { sample, 1 }, loop ? 1.f : 0.f ); // the loop input
        stateGraph.Connect( { sample, 0 }, { stateOutput, 0 } );
        return state;
    };
    const uint32_t idle = addState( "Idle", "idle", true );
    // The jump doesn't loop, so that it finishes
    const uint32_t jump = addState( "Jump", "jump", false );

    cmf::AnimationTransition start;
    start.fromState = idle;
    start.toState = jump;
    start.expression = "jumping"; // a control parameter of the network
    start.duration = 0.2f;
    graph.AddTransition( machine, start );

    cmf::AnimationTransition land;
    land.fromState = jump;
    land.toState = idle;
    land.condition = cmf::AnimationTransitionCondition::AnimationFinished;
    land.duration = 0.3f;
    graph.AddTransition( machine, land );

The first state is the start state until ``SetStartState`` changes it. ``RemoveState`` removes a state together with
its graph and its transitions; later states shift down. ``SetTransition`` and ``AddTransition`` accept expressions
that don't parse, because renaming a parameter can also break an expression; ``GetErrors`` reports them, and
``ValidateTransition`` checks a transition beforehand, for example while the user types.

CAN files are meant for playback and can't be loaded back into a builder: they only contain the parts of embedded
files that the network uses. Tools keep the editable network in their own files and export CAN files from it; CanEdit,
for example, saves ``.cane`` documents (see :doc:`../canedit/canedit`).

Only used nodes are saved, together with the graphs, assets and embedded animations they need. Unused nodes, the
graphs of unused Subgraph and StateMachine nodes, and edges to unused nodes are left out.

Node handles are not stored in the file: nodes are renumbered without removed and unused nodes when saved.
``GetSaveLayout`` returns the graphs and nodes in the order they are saved, which lets tools find nodes of the saved
file, for example to show runtime state in an editor. ``GetLayout`` numbers the graphs and nodes the same way, but
includes the unused ones; tools use it to refer to every node of the network by position, for example to keep their
own data, such as node positions, in their own files. CAN files don't store such data (see
:doc:`animationNetworkFormat`):

.. code-block:: cpp

    // Store a position per node, keyed by its location in the network
    const auto layout = builder.GetLayout();
    for( size_t graphIndex = 0; graphIndex < layout.size(); ++graphIndex )
    {
        for( size_t nodeIndex = 0; nodeIndex < layout[graphIndex].nodes.size(); ++nodeIndex )
        {
            WritePosition( graphIndex, nodeIndex, GetPosition( layout[graphIndex].nodes[nodeIndex] ) );
        }
    }
    std::vector<uint8_t> file;
    builder.Save( file );

Node Types
^^^^^^^^^^

``cmf::GetAnimationNodeTypeInfo`` describes a node type: its name, how its sockets are defined and the names, types
and default constants of its fixed inputs. Tools use it to list node types without creating nodes:

.. code-block:: cpp

    for( uint32_t type = 0; type < static_cast<uint32_t>( cmf::AnimationNodeType::Count ); ++type )
    {
        const auto info = cmf::GetAnimationNodeTypeInfo( static_cast<cmf::AnimationNodeType>( type ) );
        // info.name, info.inputs[0 .. info.inputCount), info.outputType, ...
    }

Value Kinds
^^^^^^^^^^^

Float sockets always carry a float, but some inputs only use part of the range. ``cmf::AnimationValueKind`` describes
it, so that editors can show a fitting control:

* **Number**: any value. Most Float inputs, and all inputs declared by a node (Expression inputs, subgraph interface
  sockets).
* **Boolean**: 0 for false and 1 for true, for example the ``loop`` input of AnimationSample. Networks treat any value
  other than 0 as true.
* **Integer**: whole numbers, for example the ``selector`` input of Switch. Networks truncate fractional values.

The kind is part of the node type description (``AnimationSocketInfo::valueKind``) and is not stored in files.
``AnimationGraphBuilder::GetInputValueKind`` returns the kind of any input, for example to choose an editor control:

.. code-block:: cpp

    const cmf::AnimationSocketRef input = { node, socket };
    switch( graph.GetInputValueKind( input ) )
    {
    case cmf::AnimationValueKind::Boolean: // show a checkbox
    case cmf::AnimationValueKind::Integer: // show an integer field
    case cmf::AnimationValueKind::Number:  // show a float field
        break;
    }

The kind is only a hint: ``SetInputConstant`` accepts any value, and networks evaluate any value as described above.

Embedding Assets
^^^^^^^^^^^^^^^^

``AnimationNetworkBuilder::EmbedAsset`` adds a CMF file to the network. References to its path then resolve to the
embedded file instead of going through the asset loader. This makes a network self-contained, at the cost of a
larger file.

The builder keeps the whole file, but ``Save`` only stores what the network uses from it: the skeleton, if the
skeleton reference points to the file, and the animations that AnimationSample nodes sample from it. Meshes and
unused skeletons and animations are left out, so embedding a character file with meshes doesn't add the meshes to the
network. ``Save`` fails if the network references a skeleton or animation that the embedded file doesn't contain:

.. code-block:: cpp

    builder.EmbedAsset( "characters/soldier.cmf", ReadFile( "characters/soldier.cmf" ) );
    builder.SetSkeleton( "characters/soldier.cmf", "" );
    graph.SetAnimation( idle, "characters/soldier.cmf", "idle" );

    std::vector<uint8_t> file;
    const auto saved = builder.Save( file ); // Stores the first skeleton and the "idle" animation of the file

Playing Networks
----------------

Loading
^^^^^^^

``cmf::AnimationNetwork::Load`` takes ownership of the contents of a CAN file and starts loading the external
assets. The network reads its description directly from the file buffer and doesn't allocate memory per node.
Malformed files are rejected, see `Errors`_.

.. code-block:: cpp

    #include <cmf/animationnetwork.h>

    cmf::AnimationNetwork network( loader );
    auto result = network.Load( ReadFile( "characters/soldier.can" ) );
    if( !result )
    {
        // result.error describes the problem
    }

Assets may load asynchronously. ``IsReady`` returns true once all assets are loaded and the network has no errors;
at that point the network prepares itself for evaluation. ``HasPendingAssets`` and ``GetErrors`` tell why a network
isn't ready yet. A network with warnings is ready; ``GetWarnings`` lists what it works around.

Instances
^^^^^^^^^

Create an instance per animated object once the network is ready. Every frame, set the control parameters, advance
time with ``Update`` and compute the pose with ``Evaluate``:

.. code-block:: cpp

    #include <cmf/animationnetworkinstance.h>

    cmf::AnimationNetworkInstance instance( network );
    const uint32_t speedParameter = instance.FindParameter( "speed" ); // look up once

    // Every frame
    instance.SetParameter( speedParameter, characterSpeed );
    instance.Update( deltaTime );
    instance.Evaluate();
    const cmf::Transform* localTransforms = instance.GetLocalTransforms(); // one per skeleton bone

The result contains local bone transforms. Use ``GetPose`` to copy them to a ``cmf::SkeletonPose`` and
``cmf::ComputeWorldTransforms`` to compute world transforms.

``Update`` advances the playheads of all AnimationSample nodes, including the ones that don't contribute to the
current pose, so that they stay in sync when they become active again. ``Evaluate`` only evaluates the nodes that
contribute to the result: a Blend with alpha 0 doesn't evaluate its second input, and a Switch only evaluates the
selected input.

Each instance has its own random number generator for RandomSwitch nodes; pass a seed to the constructor to get
reproducible results.

State machines take transitions in ``Evaluate``. ``Update`` advances the state time and the blends of the state
machines that contributed to the last evaluated pose. ``GetCurrentState`` returns the current state of a StateMachine
node, identified by its graph and node index in the file (the indices of ``GetSaveLayout``), for example to show it in
an editor.

The Asset Loader
----------------

Applications implement ``cmf::AnimationAssetLoader`` to load the CMF files referenced by networks. The loader is
passed to the constructors of ``AnimationNetworkBuilder`` and ``AnimationNetwork``:

.. code-block:: cpp

    class GameAssetLoader : public cmf::AnimationAssetLoader
    {
    public:
        void RequestLoad( std::string_view path, Callback onLoaded ) override
        {
            // Start loading; call onLoaded( data ) with the loaded cmf::Data, or onLoaded( nullptr ) on failure
        }

        void Release( std::string_view path ) override
        {
            // The network no longer needs the asset
        }
    };

The contract between networks and the loader is:

* **Threads**: networks call ``RequestLoad`` and ``Release`` from one thread, and the loader calls the callback on
  that same thread. The callback may be called before ``RequestLoad`` returns, for example when the asset is cached,
  or later, for example from the application's main loop when a background load completes. Networks don't use any
  synchronization.
* **Ownership**: the loader owns the loaded data. Data passed to a callback must stay valid until ``Release`` is
  called for the path.
* **Release**: every ``RequestLoad`` is matched by exactly one ``Release``, also when loading is still in progress
  or failed. Callbacks for released requests are ignored, so a loader may still deliver them.
* **Lifetime**: the loader must outlive the networks that use it.

Errors
------

Problems are reported in two ways:

* **Malformed files** fail to load: files with a bad signature, version or CRC, sections or data outside the file,
  invalid indices or node types, edges between sockets of different types, inputs with more than one incoming edge,
  cycles, or subgraphs that don't form a tree.
* **Incomplete networks** load, but aren't ready: ``GetErrors`` lists problems such as a missing skeleton, a missing
  Output node, unconnected pose inputs, and invalid expressions or random weights.
* **Missing assets** are worked around, so that networks can be evaluated while assets are missing or being
  authored: an AnimationSample node whose animation is not set, failed to load or doesn't exist plays the rest pose,
  a NamedBoneWeights node whose bone mask doesn't exist weights all bones fully, and BoneWeights entries of bones
  that don't exist are ignored. ``GetWarnings`` lists these problems.

Memory and Threads
------------------

* A loaded network keeps the file buffer and one entry per referenced asset. When it becomes ready, it prepares the
  evaluation data that all its instances share in a single allocation. Expressions are compiled to bytecode by the
  parser, which uses one allocation per expression.
* An instance makes one allocation in its constructor. ``Update`` and ``Evaluate`` don't allocate memory. The memory
  of an instance holds the parameter values, the playheads, curve evaluation caches and the intermediate poses;
  intermediate poses share memory when they are not needed at the same time.
* Instances of the same network can be used on different threads at the same time. A single instance must not be
  used by several threads at once.
* The network must outlive its instances. Loading another file into a network invalidates its instances
  (``IsValid`` returns false and ``Evaluate`` does nothing).

Adding a Node Type
------------------

Node types are defined in several places. Switch statements over node types have no default case, and the library
is built with unhandled enum values reported as errors (MSVC warning C4062, ``-Wswitch``), so the compiler lists
the places that need a new case. A new node type needs:

#. A value in ``cmf::can::NodeType`` (before ``Count``). If the node has settings, a structure for them and a
   table for it in ``cmf::can::Graph``. This changes the file format: increase ``FILE_VERSION`` and update
   :doc:`animationNetworkFormat`.
#. An entry in ``GetNodeTypeInfo`` (``src/cmf/animationnetworkformat.cpp``) describing the name, sockets and default
   input values of the node.
#. Builder support: a settings structure in the ``cmf::AnimationNodeSettings`` variant, setters in
   ``AnimationGraphBuilder``, and reading and writing the settings in the builder serializer.
#. Validation of the node data in ``AnimationNetwork::GetErrors`` and ``AnimationNetworkBuilder::GetErrors``.
#. A compiler handler (``Compiler::GetHandler`` in ``src/cmf/animationnetworkcompiler.cpp``). Nodes that produce
   poses add a pose operation; a new operation type needs an entry in ``GetPoseOpTraits`` and handlers in
   ``AnimationNetworkInstance::GetOperationHandlers`` that mark the inputs the operation needs and execute it.
#. CanEdit support (``src/canedit``): reading and writing the settings in ``networkJson.cpp``, and the palette entry,
   description, summary and diagnostics in ``app/nodeInfo.cpp``.
#. Tests and an entry in the `Node Reference`_.
