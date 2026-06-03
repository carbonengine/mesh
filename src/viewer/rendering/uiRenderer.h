#include <unordered_map>

// used by the template file
#include <imgui.h>
#include <cmf/converters.h>

#include "appState.h"
#include "rendering/renderer.h"
#include "vulkan/commandbuffer.h"

// Handles rendering the UI
class UIRenderer
{
public:
	UIRenderer( std::shared_ptr<const Renderer> );
	~UIRenderer();

	void Initialize( GLFWwindow* window, AppState& appState );
	void BeginFrame();

	void Render( AppState& appState );

	void SetupUi( AppState& appState );
	void CMFInfoWindow( AppState& appState );
	void MeshDetailsWindow( AppState& appState );
	void SetupMenubar( AppState& appState );
	void UpdateInputs( AppState& state );

private:
	template <typename T>
	struct CmfUiComboBox
	{
		std::string selectedItemName;
		T selectedItemValue;
		std::vector<std::pair<std::string, T>> items;

		void SetSelectedItemByValue( T value );
	};

	enum LoadStatus
	{
		FAILED,
		SUCCESSFUL,
		NOTHING_LOADED
	};

	struct MorphTargetUiState
	{
		std::string name{ "" };
		float weight{ 0.0f };
		bool enabled{ true };
		size_t index;
	};

	struct MeshUiState
	{
		std::string name;
		size_t meshIndex{ 0 };
		uint32_t lod{ 0 };
		uint32_t maxLodIndex{ 0 };
		float screenSize{ 0 };
		float boundingSphereRadius{ 0 };
		uint32_t vertexCount{ 0 };
		uint32_t indexCount{ 0 };
		std::vector<MorphTargetUiState> morphTargets;
		bool display{ true };
		bool boundingBox{ false };
		bool wireframeOverlay{ false };
		bool audioOcclusionMesh{ false };
		bool hasAudioOcclusionMesh{ false };
	};

	struct ModelUiState
	{
		std::vector<MeshUiState> meshes;
		CmfUiComboBox<int32_t> lod;
		uint32_t vertexCount{ 0 };
		uint32_t indexCount{ 0 };
		bool boundingBox{ false };
	};

	struct SkeletonUiState
	{
		uint32_t bonesCount{ 0 };
		std::string name;
	};

	struct SkeletonOwnerUiState
	{
		std::string source;
		std::string shortSource;
		std::shared_ptr<CmfContent> cmfContent;
		std::vector<SkeletonUiState> skeletons;
	};

	struct UiState
	{
		std::string filePath;
		ModelUiState modelStates{};
		std::vector<SkeletonOwnerUiState> skeletonOwners{};
		CmfUiComboBox<VkPolygonMode> polygonModeComboBox;
		CmfUiComboBox<std::string> visualizationShaderComboBox;
		bool boneDebug{ false };
		bool jointDebug{ false };
		bool jointAxisDebug{ false };
	};

	struct Playback
	{
		float duration{ 0.0f };
		float currentTime{ 0.0f };
		bool playing{ false };
		bool repeat{ false };
		CmfUiComboBox<std::string> animationComboBox;
	};

	struct MeshDetailsState
	{
		int selectedMeshIndex{ 0 };
		int selectedLodIndex{ 0 };
		int selectedMorphTargetIndex{ 0 };
		int indexViewMode{ 1 }; // 0 = triangles, 1 = raw
		std::unordered_map<std::string, bool> vertexAttributeFilter;
		std::unordered_map<std::string, bool> morphAttributeFilter;
		std::unordered_map<std::string, bool> boneColumnFilter;
		int linkedVertexIndex{ 0 };
		bool scrollToLinkedVertex{ true };
		int selectedIndexValue{ -1 };
		int linkedBoneIndex{ -1 };
		bool scrollToLinkedBone{ false };
		int linkedMorphTargetIndex{ -1 };
		bool navigateToLinkedMorphTarget{ false };
		int selectedAnimationIndex{ 0 };
		int linkedCurveIndex{ -1 };
		bool navigateToLinkedCurve{ false };
		std::unordered_map<std::string, bool> channelColumnFilter;
		std::unordered_map<std::string, bool> curveColumnFilter;
		std::unordered_map<std::string, bool> audioVertexColumnFilter;
	};
	void RegisterModelCallbacks( AppState& appState );

	void SetupGeneralView( AppState& appState );
	void SetupMeshListView( const ModelUiState& modelState, AppState& appState );
	void SetupMeshView( const MeshUiState& mesh, AppState& appState );
	void SetupMorphTarget( const MorphTargetUiState& morphTarget, size_t meshIndex, AppState& appState );
	void SetupSkeletonOwners( const std::vector<SkeletonOwnerUiState>& skeletonOwners, AppState& appState );
	void SetupSkeletons( const std::vector<SkeletonUiState>& skeletonStates, AppState& appState );
	void SetupPlaybackControls( AppState& appState );
	void SetupPopupWindows( AppState& appState );

	const char* GetPlaybackButtonLabel() const;
	void HandlePlaybackButtonPressed();
	void StepAnimation( float amount, AppState& appState );

	void OnChange( bool changed, std::function<void()> callback );

	template <typename T>
	void SetupCombo( const char* name, UIRenderer::CmfUiComboBox<T>& combo, State<T>& applicableState );

	void UpdateUiState( AppState& appState );
	const char* FileOpenDialog();

	struct AttributeInfo
	{
		std::string name;
		uint32_t byteOffset;
		uint8_t elementCount;
		std::pair<cmf::ConversionFunction<float>, size_t> conv;
	};

	static std::string GetUsageFlagLabel( cmf::Usage usage, uint8_t usageIndex );
	static const char* GetElementTypeName( cmf::ElementType type );

	template <typename Decl>
	static std::vector<AttributeInfo> BuildAttributes( const Decl& decl );

	void RenderAttributeTable( const char* tableId, const uint8_t* vbData, uint32_t vertexCount, uint32_t stride, const std::vector<AttributeInfo>& attributes, int scrollToVertex );
	void RenderVertexDataTab( CmfContent* cmfContent, const cmf::Mesh& mesh, const cmf::MeshLod& lod );
	void RenderIndexDataTab( CmfContent* cmfContent, const cmf::Mesh& mesh, const cmf::MeshLod& lod );
	void RenderMorphDataTab( CmfContent* cmfContent, const cmf::Mesh& mesh, const cmf::MeshLod& lod );
	void RenderBonesTab( CmfContent* cmfContent, const cmf::Mesh& mesh, AppState& appState );
	void RenderHierarchyTab( CmfContent* cmfContent );
	void RenderAnimationsTab( CmfContent* cmfContent );
	void RenderAnimationChannelsSubTab( const cmf::Animation& anim );
	void RenderAnimationCurvesSubTab( const cmf::Animation& anim );
	void RenderAudioOccluderTab( const cmf::Mesh& mesh );

	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };

	std::shared_ptr<const Renderer> m_renderer;
	GraphicsCommandBuffer m_graphicsCommandBuffer;

	UiState m_uiState{};
	LoadStatus m_loadStatus{ NOTHING_LOADED };
	Playback m_playback{};
	MeshDetailsState m_meshDetailsState{};
};


#include "uiRenderer_template_impl.h"