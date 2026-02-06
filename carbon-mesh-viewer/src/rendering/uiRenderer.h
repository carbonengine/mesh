
#include <imgui.h>

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
		uint32_t lodCount{ 0 };
		uint32_t vertexCount{ 0 };
		uint32_t indexCount{ 0 };
		bool display{ true };
		std::vector<MorphTargetUiState> morphTargets;
	};

	struct ModelUiState
	{
		std::vector<MeshUiState> meshes;
		uint32_t selectedLod{ 0 };
		uint32_t totalVertexCount{ 0 };
		uint32_t totalIndexCount{ 0 };
	};

	struct UiState
	{
		std::string filePath;
		ModelUiState modelStates{};

		CmfUiComboBox<VkPolygonMode> polygonModeComboBox;
		CmfUiComboBox<std::string> visualizationShaderComboBox;
	};

	void SetupGeneralView( AppState& appState );
	void SetupMeshListView( const ModelUiState& modelState, AppState& appState );
	void SetupMeshView( const MeshUiState& mesh, AppState& appState );
	void SetupMorphTarget( const MorphTargetUiState& morphTarget, AppState& appState );

	void OnChange( bool changed, std::function<void()> callback );

	template <typename T>
	void SetupCombo( const char* name, UIRenderer::CmfUiComboBox<T>& combo, State<T>& applicableState );

	void UpdateUiState( AppState& appState );
	void FileOpenDialog( AppState& appState );

	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };

	std::shared_ptr<const Renderer> m_renderer;
	CommandBuffer m_commandBuffer;

	UiState m_uiState{};
	bool m_cmfFullReset{ false };
	bool m_cmfAttributeChange{ false };
};


#include "uiRenderer_template_impl.h"