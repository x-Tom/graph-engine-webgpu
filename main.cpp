// #include <emscripten.h>
// #include <webgpu/webgpu.h>

#include "webgpu-utils.h"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <cassert>
#include <string>
#include <cmath>
#include <unordered_map>

#include "Application.h"

// #include "glm/glm.hpp"
int main (int, char**) {

	Application app;
	if (!app.onInit()) return 1;

	std::cout << "Maximum Vertex Buffer size: " << app.m_supported_limits.limits.maxBufferSize << " bytes\t" << app.m_supported_limits.limits.maxBufferSize/44 << " VAStructs" << std::endl;
	std::cout << "Current Vertex Buffer size: " << app.m_current_limits.limits.maxBufferSize << " bytes\t" << app.m_current_limits.limits.maxBufferSize/44 << " VAStructs" << std::endl;


	while (app.isRunning()) {
		app.onFrame();
	}

	app.onFinish();

    return 0;
}
