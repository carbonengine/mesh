#pragma once

#include <stdio.h>
#include <iostream>
#include <GLFW/glfw3.h>

#include "rendercontext.h"
#include "inputhandler.h"

class CarbonMeshViewerApp {
public:
    CarbonMeshViewerApp();
    ~CarbonMeshViewerApp();
    
    void run();
    
private:
	RenderContext* m_renderContext = nullptr;
    InputHandler* m_inputHandler = nullptr;
	GLFWwindow* m_window = nullptr;
};