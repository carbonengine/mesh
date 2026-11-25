// Copyright © 2026 CCP ehf.

#include "animationnetworkcompiled.h"

#include "animationinternal.h"
#include "animationnetworkformat.h"

#include <algorithm>
#include <cstring>
#include <numeric>

namespace cmf::detail
{
namespace
{

/** A node in the flattened network: a node of one of the graphs. */
struct NodeRef
{
	uint32_t graph = can::NO_INDEX;
	uint32_t node = can::NO_INDEX;

	bool operator==( const NodeRef& other ) const
	{
		return graph == other.graph && node == other.node;
	}
};

/** Where the value of an input socket comes from after inlining subgraphs. */
struct Source
{
	enum class Kind : uint8_t
	{
		/** The input is not connected and has no constant (pose and bone weight inputs). */
		None,
		/** The input is not connected; it uses a constant. */
		Constant,
		/** The input is connected to the output of a node. */
		Node,
	};

	Kind kind = Kind::None;
	NodeRef node;
	float constant = 0.f;
};

/** Collects the operations and data of the network before they are copied into the memory block. */
class Compiler
{
public:
	Compiler( const AnimationNetwork& network, std::vector<std::string>& errors ) :
		m_network( &network ),
		m_data( network.GetData() ),
		m_skeleton( network.GetSkeleton() ),
		m_boneCount( static_cast<uint32_t>( m_skeleton->bones.size() ) ),
		m_errors( &errors )
	{
		m_parents.resize( m_data->graphs.size() );
		m_results.resize( m_data->graphs.size() );
		for( uint32_t g = 0; g < m_data->graphs.size(); ++g )
		{
			const auto& graph = m_data->graphs[g];
			m_results[g].assign( graph.nodes.size(), can::NO_INDEX );
			for( uint32_t n = 0; n < graph.nodes.size(); ++n )
			{
				if( graph.nodes[n].type == can::NodeType::Subgraph )
				{
					m_parents[GetNodeData<can::SubgraphNode>( graph, graph.nodes[n] ).graphIndex] = { g, n };
				}
			}
		}
	}

	std::unique_ptr<CompiledNetwork> Compile()
	{
		const auto& root = m_data->graphs[0];
		const auto output = ResolveInput( { 0, root.interfaceOutputs[0] }, 0 );
		VisitDependencies( output.node );
		const auto firstExpressionSlot = static_cast<uint32_t>( m_data->parameters.size() + m_constants.size() );
		ResolveExpressionSlots( firstExpressionSlot );
		ParseExpressions();
		if( !m_errors->empty() )
		{
			return nullptr;
		}
		AssignPoseSlots();
		return Build();
	}

private:
	const AnimationNetwork* m_network;
	const can::NetworkData* m_data;
	const Skeleton* m_skeleton;
	uint32_t m_boneCount;
	std::vector<std::string>* m_errors;

	/** For each subgraph, the Subgraph node that contains it. */
	std::vector<NodeRef> m_parents;
	/** For each node, the index of its pose operation, float slot or bone weight array, depending on its type. */
	std::vector<std::vector<uint32_t>> m_results;

	std::vector<float> m_constants;
	std::vector<NodeRef> m_expressionNodes;
	std::vector<std::vector<uint32_t>> m_expressionInputs;
	std::vector<PoseOp> m_poseOps;
	std::vector<uint32_t> m_switchInputs;
	std::vector<SampleInfo> m_samples;
	std::vector<CurveBinding<Vector3>> m_vectorCurves;
	std::vector<CurveBinding<Vector4>> m_quaternionCurves;
	std::vector<RandomSwitchInfo> m_randomSwitches;
	std::vector<float> m_randomThresholds;
	std::vector<float> m_boneWeights;
	std::vector<CcpParser::Program> m_programs;
	std::vector<FloatOp> m_floatOps;
	uint32_t m_poseSlotCount = 0;

	[[nodiscard]] const can::Graph& GetGraph( const NodeRef& ref ) const
	{
		return m_data->graphs[ref.graph];
	}

	[[nodiscard]] const can::Node& GetNode( const NodeRef& ref ) const
	{
		return GetGraph( ref ).nodes[ref.node];
	}

	// Follows edges through SubgraphInput, Subgraph and Output nodes to the node producing the value of an input
	[[nodiscard]] Source ResolveInput( NodeRef node, uint32_t socket ) const
	{
		while( true )
		{
			const auto& graph = GetGraph( node );
			const auto* edge = std::find_if( graph.edges.begin(), graph.edges.end(), [&node, socket]( const can::Edge& candidate ) { return candidate.to.node == node.node && candidate.to.socket == socket; } );
			if( edge == graph.edges.end() )
			{
				const auto& target = GetNode( node );
				if( GetInputType( *m_data, graph, target, socket ) != can::SocketType::Float )
				{
					return {};
				}
				return { Source::Kind::Constant, {}, target.inputConstants[socket] };
			}
			const NodeRef from = { node.graph, edge->from.node };
			const auto& fromNode = GetNode( from );
			if( fromNode.type == can::NodeType::SubgraphInput )
			{
				// Continue with the matching input of the Subgraph node in the parent graph
				const auto& inputs = graph.interfaceInputs;
				socket = static_cast<uint32_t>( std::find( inputs.begin(), inputs.end(), from.node ) - inputs.begin() );
				node = m_parents[node.graph];
			}
			else if( fromNode.type == can::NodeType::Subgraph )
			{
				// Continue with the Output node of the subgraph
				const auto subgraphIndex = GetNodeData<can::SubgraphNode>( graph, fromNode ).graphIndex;
				node = { subgraphIndex, m_data->graphs[subgraphIndex].interfaceOutputs[edge->from.socket] };
				socket = 0;
			}
			else
			{
				return { Source::Kind::Node, from, 0.f };
			}
		}
	}

	// Number of inputs whose sources a node depends on
	[[nodiscard]] uint32_t GetDependencyCount( const NodeRef& ref ) const
	{
		const auto& node = GetNode( ref );
		return GetInputCount( *m_data, GetGraph( ref ), node );
	}

	// Visits the producers of the output and their dependencies; producers are processed after their dependencies
	void VisitDependencies( const NodeRef& output )
	{
		struct Frame
		{
			NodeRef node;
			uint32_t nextInput = 0;
		};
		std::vector<Frame> stack = { { output, 0 } };
		while( !stack.empty() )
		{
			auto& frame = stack.back();
			if( frame.nextInput < GetDependencyCount( frame.node ) )
			{
				const auto source = ResolveInput( frame.node, frame.nextInput++ );
				if( source.kind == Source::Kind::Node && m_results[source.node.graph][source.node.node] == can::NO_INDEX )
				{
					stack.push_back( { source.node, 0 } );
				}
				continue;
			}
			const auto node = frame.node;
			stack.pop_back();
			if( m_results[node.graph][node.node] == can::NO_INDEX )
			{
				m_results[node.graph][node.node] = AddProducer( node );
			}
		}
	}

	// Returns the float slot holding the value of an input
	uint32_t GetFloatSlot( const NodeRef& node, uint32_t socket )
	{
		const auto source = ResolveInput( node, socket );
		if( source.kind == Source::Kind::Node )
		{
			return m_results[source.node.graph][source.node.node];
		}
		m_constants.push_back( source.constant );
		return static_cast<uint32_t>( m_data->parameters.size() + m_constants.size() - 1 );
	}

	[[nodiscard]] uint32_t GetPoseOp( const NodeRef& node, uint32_t socket ) const
	{
		const auto source = ResolveInput( node, socket );
		return m_results[source.node.graph][source.node.node];
	}

	[[nodiscard]] uint32_t GetBoneWeights( const NodeRef& node, uint32_t socket ) const
	{
		const auto source = ResolveInput( node, socket );
		return source.kind == Source::Kind::Node ? m_results[source.node.graph][source.node.node] : can::NO_INDEX;
	}

	/** Creates the operation or data of a node whose dependencies are processed and returns its result index: a
	* float slot, a pose operation or a bone weight array, depending on the node type. */
	using Handler = uint32_t ( Compiler::* )( const NodeRef& );

	static Handler GetHandler( can::NodeType type )
	{
		// Every node type needs a case; there is deliberately no default case
		switch( type )
		{
		case can::NodeType::Parameter:
			return &Compiler::AddParameter;
		case can::NodeType::Expression:
			return &Compiler::AddExpression;
		case can::NodeType::NamedBoneWeights:
			return &Compiler::AddNamedBoneWeights;
		case can::NodeType::BoneWeights:
			return &Compiler::AddBoneWeights;
		case can::NodeType::AnimationSample:
			return &Compiler::AddSampleOp;
		case can::NodeType::RestPose:
			return &Compiler::AddRestPoseOp;
		case can::NodeType::Blend:
			return &Compiler::AddBlendOp;
		case can::NodeType::AdditiveBlend:
			return &Compiler::AddAdditiveBlendOp;
		case can::NodeType::Switch:
			return &Compiler::AddSwitchOp;
		case can::NodeType::RandomSwitch:
			return &Compiler::AddRandomSwitchOp;
		case can::NodeType::Output:
		case can::NodeType::SubgraphInput:
		case can::NodeType::Subgraph:
		case can::NodeType::Count:
			// Interface and Subgraph nodes are followed by ResolveInput and never produce values themselves
			break;
		}
		return nullptr;
	}

	uint32_t AddProducer( const NodeRef& ref )
	{
		const auto handler = GetHandler( GetNode( ref ).type );
		return handler ? ( this->*handler )( ref ) : can::NO_INDEX;
	}

	uint32_t AddParameter( const NodeRef& ref )
	{
		// Parameters occupy the first float slots
		return GetNodeData<can::ParameterNode>( GetGraph( ref ), GetNode( ref ) ).parameterIndex;
	}

	uint32_t AddExpression( const NodeRef& ref )
	{
		const auto& data = GetNodeData<can::ExpressionNode>( GetGraph( ref ), GetNode( ref ) );
		std::vector<uint32_t> inputSlots;
		inputSlots.reserve( data.inputNames.size() );
		for( uint32_t i = 0; i < data.inputNames.size(); ++i )
		{
			inputSlots.push_back( GetFloatSlot( ref, i ) );
		}
		// Expressions are parsed once the final float slot indices are known, see ParseExpressions
		const auto index = static_cast<uint32_t>( m_floatOps.size() );
		m_expressionNodes.push_back( ref );
		m_expressionInputs.push_back( std::move( inputSlots ) );
		m_floatOps.push_back( { index, index } );
		return EXPRESSION_SLOT_FLAG | index;
	}

	// Parses the expressions with variables bound to the float slots of their inputs
	void ParseExpressions()
	{
		m_programs.resize( m_expressionNodes.size() );
		for( size_t e = 0; e < m_expressionNodes.size(); ++e )
		{
			const auto& data = GetNodeData<can::ExpressionNode>( GetGraph( m_expressionNodes[e] ), GetNode( m_expressionNodes[e] ) );
			std::vector<std::string> names;
			std::vector<CcpParser::Variable> variables;
			names.reserve( data.inputNames.size() );
			variables.reserve( data.inputNames.size() );
			for( uint32_t i = 0; i < data.inputNames.size(); ++i )
			{
				names.push_back( ToStdString( data.inputNames[i] ) );
				variables.push_back( { names.back().c_str(), 0, static_cast<CcpParser::OffsetType>( m_expressionInputs[e][i] * sizeof( float ) ) } );
			}
			std::array<CcpParser::VariableView, 1> variableViews;
			variableViews[0] = variables;
			CcpParser::Externals externals;
			externals.variables.data = variableViews.data();
			externals.variables.count = variableViews.size();

			const auto text = ToStdString( data.expression );
			const auto result = CcpParser::Parse( text.c_str(), externals, m_programs[e] );
			if( !result )
			{
				m_errors->push_back( "Invalid expression '" + text + "': " + CcpParser::ToString( result, text.c_str() ) );
			}
		}
	}

	uint32_t AddNamedBoneWeights( const NodeRef& ref )
	{
		const auto& data = GetNodeData<can::NamedBoneWeightsNode>( GetGraph( ref ), GetNode( ref ) );
		const auto index = static_cast<uint32_t>( m_boneWeights.size() / m_boneCount );
		const auto* mask = FindBoneMask( *m_skeleton, ToStdStringView( data.maskName ) );
		if( mask == nullptr )
		{
			// A missing mask weights all bones fully, like an unconnected weights input
			m_boneWeights.resize( m_boneWeights.size() + m_boneCount, 1.f );
			return index;
		}
		m_boneWeights.resize( m_boneWeights.size() + m_boneCount, 0.f );
		for( const auto& weight : mask->weights )
		{
			if( weight.index < m_boneCount )
			{
				m_boneWeights[index * m_boneCount + weight.index] = weight.weight;
			}
		}
		return index;
	}

	uint32_t AddBoneWeights( const NodeRef& ref )
	{
		const auto& data = GetNodeData<can::BoneWeightsNode>( GetGraph( ref ), GetNode( ref ) );
		const auto index = static_cast<uint32_t>( m_boneWeights.size() / m_boneCount );
		m_boneWeights.resize( m_boneWeights.size() + m_boneCount, data.defaultWeight );
		for( const auto& weight : data.weights )
		{
			const auto bone = FindBone( *m_skeleton, ToStdStringView( weight.bone ) );
			// Weights of bones that are not in the skeleton are ignored
			if( bone != can::NO_INDEX )
			{
				m_boneWeights[index * m_boneCount + bone] = weight.weight;
			}
		}
		return index;
	}

	uint32_t PushPoseOp( const PoseOp& op )
	{
		m_poseOps.push_back( op );
		return static_cast<uint32_t>( m_poseOps.size() - 1 );
	}

	uint32_t AddSampleOp( const NodeRef& ref )
	{
		const auto& data = GetNodeData<can::AnimationSampleNode>( GetGraph( ref ), GetNode( ref ) );
		if( m_network->GetAnimation( data ) == nullptr )
		{
			// Animations that are not set, failed to load or are missing play the rest pose
			return AddRestPoseOp( ref );
		}
		PoseOp op;
		op.type = PoseOpType::Sample;
		op.data = AddSample( ref );
		return PushPoseOp( op );
	}

	uint32_t AddRestPoseOp( const NodeRef& /*ref*/ )
	{
		PoseOp op;
		op.type = PoseOpType::RestPose;
		op.outputSlot = REST_POSE_SLOT;
		return PushPoseOp( op );
	}

	uint32_t AddBlendOp( const NodeRef& ref )
	{
		PoseOp op;
		op.type = PoseOpType::Blend;
		op.inputs[0] = GetPoseOp( ref, BLEND_POSE_A_INPUT );
		op.inputs[1] = GetPoseOp( ref, BLEND_POSE_B_INPUT );
		op.floatSlot = GetFloatSlot( ref, BLEND_ALPHA_INPUT );
		op.boneWeights = GetBoneWeights( ref, BLEND_WEIGHTS_INPUT );
		return PushPoseOp( op );
	}

	uint32_t AddAdditiveBlendOp( const NodeRef& ref )
	{
		PoseOp op;
		op.type = PoseOpType::AdditiveBlend;
		op.inputs[0] = GetPoseOp( ref, ADDITIVE_POSE_INPUT );
		op.inputs[1] = GetPoseOp( ref, ADDITIVE_BASE_INPUT );
		op.inputs[2] = GetPoseOp( ref, ADDITIVE_ADDITIVE_INPUT );
		op.floatSlot = GetFloatSlot( ref, ADDITIVE_ALPHA_INPUT );
		op.boneWeights = GetBoneWeights( ref, ADDITIVE_WEIGHTS_INPUT );
		return PushPoseOp( op );
	}

	uint32_t AddSwitchOp( const NodeRef& ref )
	{
		PoseOp op;
		op.type = PoseOpType::Switch;
		op.floatSlot = GetFloatSlot( ref, SWITCH_SELECTOR_INPUT );
		AddSwitchInputs( ref, op );
		return PushPoseOp( op );
	}

	uint32_t AddRandomSwitchOp( const NodeRef& ref )
	{
		PoseOp op;
		op.type = PoseOpType::RandomSwitch;
		op.data = AddRandomSwitch( GetNodeData<can::RandomSwitchNode>( GetGraph( ref ), GetNode( ref ) ) );
		AddSwitchInputs( ref, op );
		return PushPoseOp( op );
	}

	void AddSwitchInputs( const NodeRef& ref, PoseOp& op )
	{
		const auto shape = GetShape( GetGraph( ref ), GetNode( ref ) );
		op.firstInput = static_cast<uint32_t>( m_switchInputs.size() );
		op.inputCount = shape.poseInputCount;
		const auto firstSocket = GetFirstSwitchPoseInput( shape.type );
		for( uint32_t i = 0; i < shape.poseInputCount; ++i )
		{
			m_switchInputs.push_back( GetPoseOp( ref, firstSocket + i ) );
		}
	}

	uint32_t AddRandomSwitch( const can::RandomSwitchNode& data )
	{
		RandomSwitchInfo info;
		info.firstThreshold = static_cast<uint32_t>( m_randomThresholds.size() );
		info.inputCount = data.poseInputCount;
		info.rerollOnActivate = ( data.flags & can::RANDOM_SWITCH_REROLL_ON_ACTIVATE ) != 0;
		const float total = data.weights.empty() ? static_cast<float>( data.poseInputCount ) : std::accumulate( data.weights.begin(), data.weights.end(), 0.f );
		float sum = 0.f;
		for( uint32_t i = 0; i < data.poseInputCount; ++i )
		{
			sum += data.weights.empty() ? 1.f : data.weights[i];
			m_randomThresholds.push_back( sum / total );
		}
		m_randomSwitches.push_back( info );
		return static_cast<uint32_t>( m_randomSwitches.size() - 1 );
	}

	uint32_t AddSample( const NodeRef& ref )
	{
		const auto& data = GetNodeData<can::AnimationSampleNode>( GetGraph( ref ), GetNode( ref ) );
		const auto& animation = *m_network->GetAnimation( data );

		SampleInfo sample;
		sample.duration = animation.duration;
		sample.timeConnected = ResolveInput( ref, SAMPLE_TIME_INPUT ).kind == Source::Kind::Node;
		sample.timeSlot = GetFloatSlot( ref, SAMPLE_TIME_INPUT );
		sample.loopSlot = GetFloatSlot( ref, SAMPLE_LOOP_INPUT );
		sample.speedSlot = GetFloatSlot( ref, SAMPLE_SPEED_INPUT );
		sample.firstVectorCurve = static_cast<uint32_t>( m_vectorCurves.size() );
		sample.firstQuaternionCurve = static_cast<uint32_t>( m_quaternionCurves.size() );
		for( const auto& channel : animation.channels )
		{
			const auto bone = FindBone( *m_skeleton, ToStdStringView( channel.target ) );
			if( !IsBoneTarget( channel.targetType ) || bone == can::NO_INDEX || channel.curveIndex >= animation.curves.size() )
			{
				// Channels of bones that are not in the network skeleton are ignored
				continue;
			}
			const auto& curve = animation.curves[channel.curveIndex];
			if( channel.targetType == AnimationChannelTargetType::BoneRotation )
			{
				m_quaternionCurves.push_back( { GetKnotStream( curve ), GetValueStream<Vector4>( curve ), curve.interpolation, false, bone } );
			}
			else
			{
				const bool scale = channel.targetType == AnimationChannelTargetType::BoneScale;
				m_vectorCurves.push_back( { GetKnotStream( curve ), GetValueStream<Vector3>( curve ), curve.interpolation, scale, bone } );
			}
		}
		sample.vectorCurveCount = static_cast<uint32_t>( m_vectorCurves.size() ) - sample.firstVectorCurve;
		sample.quaternionCurveCount = static_cast<uint32_t>( m_quaternionCurves.size() ) - sample.firstQuaternionCurve;
		m_samples.push_back( sample );
		return static_cast<uint32_t>( m_samples.size() - 1 );
	}


	template <typename Visitor>
	void ForEachPoseInput( const PoseOp& op, const Visitor& visitor ) const // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
	{
		for( const auto input : op.inputs )
		{
			if( input != can::NO_INDEX )
			{
				visitor( input );
			}
		}
		for( uint32_t i = 0; i < op.inputCount; ++i )
		{
			visitor( m_switchInputs[op.firstInput + i] );
		}
	}

	// Assigns pose slots so that slots are reused once the poses they hold are no longer needed
	void AssignPoseSlots()
	{
		const auto opCount = static_cast<uint32_t>( m_poseOps.size() );
		// Index of the last operation reading the output of each operation; the output pose is read after all of them
		std::vector<uint32_t> lastUse( opCount, 0 );
		lastUse.back() = opCount;
		for( uint32_t i = 0; i < opCount; ++i )
		{
			ForEachPoseInput( m_poseOps[i], [&lastUse, i]( uint32_t input ) { lastUse[input] = std::max( lastUse[input], i ); } );
		}
		// An operation passing an input through extends the lifetime of that input to its own
		for( uint32_t i = opCount; i-- > 0; )
		{
			if( GetPoseOpTraits( m_poseOps[i].type ).mayPassThrough )
			{
				ForEachPoseInput( m_poseOps[i], [&lastUse, i]( uint32_t input ) { lastUse[input] = std::max( lastUse[input], lastUse[i] ); } );
			}
		}

		std::vector<uint32_t> freeSlots;
		std::vector<std::pair<uint32_t, uint32_t>> usedSlots; // (last use, slot)
		for( uint32_t i = 0; i < opCount; ++i )
		{
			auto& op = m_poseOps[i];
			for( auto it = usedSlots.begin(); it != usedSlots.end(); )
			{
				if( it->first < i )
				{
					freeSlots.push_back( it->second );
					it = usedSlots.erase( it );
				}
				else
				{
					++it;
				}
			}
			if( !GetPoseOpTraits( op.type ).ownsSlot )
			{
				continue;
			}
			if( freeSlots.empty() )
			{
				freeSlots.push_back( m_poseSlotCount++ );
			}
			op.outputSlot = freeSlots.back();
			freeSlots.pop_back();
			usedSlots.emplace_back( lastUse[i], op.outputSlot );
		}
	}

	std::unique_ptr<CompiledNetwork> Build()
	{
		auto result = std::make_unique<CompiledNetwork>();
		auto& compiled = *result;
		compiled.skeleton = m_skeleton;
		compiled.boneCount = m_boneCount;
		compiled.parameters = m_data->parameters.data();
		compiled.parameterCount = static_cast<uint32_t>( m_data->parameters.size() );
		compiled.constantCount = static_cast<uint32_t>( m_constants.size() );
		compiled.floatSlotCount = compiled.parameterCount + compiled.constantCount + static_cast<uint32_t>( m_floatOps.size() );
		for( const auto& program : m_programs )
		{
			compiled.arenaSize = std::max( compiled.arenaSize, program.GetTempArenaSize() );
		}
		compiled.programs = std::move( m_programs );

		BlockLayout layout;
		const auto constants = layout.Add<float>( m_constants.size() );
		const auto floatOps = layout.Add<FloatOp>( m_floatOps.size() );
		const auto poseOps = layout.Add<PoseOp>( m_poseOps.size() );
		const auto switchInputs = layout.Add<uint32_t>( m_switchInputs.size() );
		const auto samples = layout.Add<SampleInfo>( m_samples.size() );
		const auto vectorCurves = layout.Add<CurveBinding<Vector3>>( m_vectorCurves.size() );
		const auto quaternionCurves = layout.Add<CurveBinding<Vector4>>( m_quaternionCurves.size() );
		const auto randomSwitches = layout.Add<RandomSwitchInfo>( m_randomSwitches.size() );
		const auto randomThresholds = layout.Add<float>( m_randomThresholds.size() );
		const auto boneWeights = layout.Add<float>( m_boneWeights.size() );
		compiled.block.reset( new std::byte[layout.GetSize()] ); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): raw memory block owned by the unique_ptr

		compiled.constants = Copy( compiled, constants, m_constants );
		compiled.floatOps = Copy( compiled, floatOps, m_floatOps );
		compiled.floatOpCount = static_cast<uint32_t>( m_floatOps.size() );
		compiled.poseOps = Copy( compiled, poseOps, m_poseOps );
		compiled.poseOpCount = static_cast<uint32_t>( m_poseOps.size() );
		compiled.switchInputs = Copy( compiled, switchInputs, m_switchInputs );
		compiled.poseSlotCount = m_poseSlotCount;
		compiled.samples = Copy( compiled, samples, m_samples );
		compiled.sampleCount = static_cast<uint32_t>( m_samples.size() );
		compiled.vectorCurves = Copy( compiled, vectorCurves, m_vectorCurves );
		compiled.vectorCurveCount = static_cast<uint32_t>( m_vectorCurves.size() );
		compiled.quaternionCurves = Copy( compiled, quaternionCurves, m_quaternionCurves );
		compiled.quaternionCurveCount = static_cast<uint32_t>( m_quaternionCurves.size() );
		compiled.randomSwitches = Copy( compiled, randomSwitches, m_randomSwitches );
		compiled.randomSwitchCount = static_cast<uint32_t>( m_randomSwitches.size() );
		compiled.randomThresholds = Copy( compiled, randomThresholds, m_randomThresholds );
		compiled.boneWeights = Copy( compiled, boneWeights, m_boneWeights );
		compiled.instanceLayout = ComputeInstanceLayout( compiled );
		return result;
	}

	template <typename T>
	static const T* Copy( CompiledNetwork& compiled, size_t offset, const std::vector<T>& source ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
	{
		static_assert( std::is_trivially_destructible_v<T>, "Block arrays are never destroyed" );
		auto* target = reinterpret_cast<T*>( compiled.block.get() + offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): the layout reserved properly aligned space for the array
		for( size_t i = 0; i < source.size(); ++i )
		{
			new( target + i ) T( source[i] ); // NOLINT(cppcoreguidelines-owning-memory): objects live in the block and are trivially destructible
		}
		return target;
	}

	// Replaces the expression slot placeholders with final float slot indices
	void ResolveExpressionSlots( uint32_t firstExpressionSlot )
	{
		const auto resolve = [firstExpressionSlot]( uint32_t& slot ) {
			if( slot != can::NO_INDEX && ( slot & EXPRESSION_SLOT_FLAG ) != 0 )
			{
				slot = firstExpressionSlot + ( slot & ~EXPRESSION_SLOT_FLAG );
			}
		};
		for( auto& op : m_floatOps )
		{
			op.outputSlot += firstExpressionSlot;
		}
		for( auto& op : m_poseOps )
		{
			resolve( op.floatSlot );
		}
		for( auto& sample : m_samples )
		{
			resolve( sample.timeSlot );
			resolve( sample.loopSlot );
			resolve( sample.speedSlot );
		}
		for( auto& inputs : m_expressionInputs )
		{
			std::for_each( inputs.begin(), inputs.end(), resolve );
		}
	}

	static constexpr uint32_t EXPRESSION_SLOT_FLAG = 0x80000000;
};

}

InstanceLayout ComputeInstanceLayout( const CompiledNetwork& compiled )
{
	BlockLayout layout;
	InstanceLayout result;
	result.floatSlots = layout.Add<float>( compiled.floatSlotCount );
	result.playheads = layout.Add<Playhead>( compiled.sampleCount );
	result.vectorCaches = layout.Add<CurveEvaluationCache<Vector3>>( compiled.vectorCurveCount );
	result.quaternionCaches = layout.Add<CurveEvaluationCache<Vector4>>( compiled.quaternionCurveCount );
	result.randomStates = layout.Add<RandomSwitchState>( compiled.randomSwitchCount );
	result.opOutputs = layout.Add<const Transform*>( compiled.poseOpCount );
	result.opFlags = layout.Add<uint8_t>( compiled.poseOpCount );
	result.poseSlots = layout.Add<Transform>( static_cast<size_t>( compiled.poseSlotCount ) * compiled.boneCount );
	result.arena = layout.Add<std::byte>( compiled.arenaSize );
	result.size = layout.GetSize();
	return result;
}

std::unique_ptr<CompiledNetwork> CompileNetwork( const AnimationNetwork& network, std::vector<std::string>& errors )
{
	Compiler compiler( network, errors );
	return compiler.Compile();
}

}
