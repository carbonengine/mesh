extern const char* g_moduleName = "CarbonMeshViewer";

#include <CCPLog.h>
#include "app.h"

int main()
{
	auto app = CarbonMeshViewerApp();
    app.run();
}