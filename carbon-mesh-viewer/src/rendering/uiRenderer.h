
#include <imgui.h>

#include "appState.h"
#include "rendering/renderer.h"

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

	struct CmfUiState
	{
		std::string filePath;

		int32_t selectedMesh;
		CmfUiComboBox<int32_t> meshComboBox;
		CmfUiComboBox<uint32_t> lodComboBox;

		uint32_t vertexCount{ 0 };
		uint32_t indexCount{ 0 };

		CmfUiComboBox<VkPolygonMode> polygonModeComboBox;
		CmfUiComboBox<std::string> visualizationShaderComboBox;
	};

	template <typename T>
	void SetupCombo( const char* name, UIRenderer::CmfUiComboBox<T>& combo, State<T>& applicableState );

	void UpdateUiState( AppState& appState );
	void FileOpenDialog( AppState& appState );

	VkDescriptorPool m_DescriptorPool{ VK_NULL_HANDLE };

	std::shared_ptr<const Renderer> m_renderer;
	CmfUiState m_uiState{};
	bool m_cmfFullReset{ false };
	bool m_cmfAttributeChange{ false };
};


#include "uiRenderer_template_impl.h"