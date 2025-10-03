#pragma once

#include "input/inputhandler.h"
#include "vulkan/renderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Application
{
public:
	Application();
	~Application();

	void init();
	void run();

private:
	GLFWwindow* m_window;
	InputHandler* m_inputHandler;
	Renderer* m_renderer;
	bool callUpdate = false;
};