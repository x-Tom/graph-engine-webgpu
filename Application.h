/**
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://github.com/eliemichel/LearnWebGPU
 * 
 * MIT License
 * Copyright (c) 2022-2023 Elie Michel
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>

#include <array>
#include "webgpu-utils.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>


// Forward declare
struct GLFWwindow;

class Application {
public:
	// A function called only once at the beginning. Returns false is init failed.
	bool onInit();

	// A function called at each frame, guaranteed never to be called before `onInit`.
	void onFrame();

	// A function called only once at the very end.
	void onFinish();

	// A function that tells if the application is still running.
	bool isRunning();

	// A function called when the window is resized.
	void onResize();

	// Mouse events
	void onMouseMove(double xpos, double ypos);
	void onMouseButton(int button, int action, int mods);
	void onScroll(double xoffset, double yoffset);

	WGPUSupportedLimits m_supported_limits;
	WGPURequiredLimits m_current_limits;


private:
	bool initWindowAndDevice();
	void terminateWindowAndDevice();

	// Init swap chain
	bool initSwapChain();
	void terminateSwapChain();

	//Init depth buffer
	bool initDepthBuffer();
	void terminateDepthBuffer();

	// Init Boat Render Pipeline
	// bool initRenderPipeline();
	bool initRenderPipeline(const std::string& pipelineName, const std::string& shaderFileName, WGPUPrimitiveTopology topology);
	void terminateRenderPipeline(const std::string& pipelineName);
	void terminateRenderPipelines();


	// Init texture
	bool initTexture();
	void terminateTexture();

	// Boat
	bool initGeometry();
	void terminateGeometry();

	bool initArrowGeometry(float length, float headLength, float headAngle);

	// Graph objects (parametric curves, surfaces, vector fields)
	bool initGraphObjects();
	void terminateGraphObjects();
	void updateGraphObjects();

	// Axes
	bool initAxesGeometry();
	bool initXYPlaneWireframeGeometry(float spacing, float len = 1000);
	bool initPrincipalPlanesWireframeGeometry(float spacing, float len = 1000);
	bool initWireframeGeometry(float spacing, float len = 500);
	void terminateAxesGeometry();

	bool initUniforms(float fovy = glm::radians(45.0f), float aspectRatio = 640.0f / 480.0f, float nearPlane = 0.01f, float farPlane = 100.0f);
	void terminateUniforms();

	bool initLightingUniforms();
	void terminateLightingUniforms();
	void updateLightingUniforms();

	bool initBindGroupLayout();
	void terminateBindGroupLayout();

	bool initBindGroup();
	void terminateBindGroup();

	void updateProjectionMatrix();
	void updateViewMatrix();
	void updateDragInertia();

	bool initGui(); // called in onInit
	void terminateGui(); // called in onFinish
	void updateGui(WGPURenderPassEncoder renderPass); // called in onFrame

private:
	// (Just aliases to make notations lighter)
	using mat4x4 = glm::mat4x4;
	using vec4 = glm::vec4;
	using vec3 = glm::vec3;
	using vec2 = glm::vec2;

	/**
	 * The same structure as in the shader, replicated in C++
	 */
	struct MyUniforms {
		// We add transform matrices
		mat4x4 projectionMatrix;
		mat4x4 viewMatrix;
		mat4x4 modelMatrix;
		vec4 color;
		vec3 cameraWorldPosition;
		float time;
	};
	// Have the compiler check byte alignment
	static_assert(sizeof(MyUniforms) % 16 == 0);

	struct LightingUniforms {
		std::array<vec4, 2> directions;
		std::array<vec4, 2> colors;

		// Material properties
		float hardness = 32.0f;
		float kd = 1.0f;
		float ks = 0.5f;

		float _pad[1];
	};
	static_assert(sizeof(LightingUniforms) % 16 == 0);

	struct CameraState {
		// angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
		// angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
		vec2 angles = { 0.8f, 0.5f };
		// zoom is the position of the camera along its local forward axis, affected by the scroll wheel
		float zoom = -1.2f;

		vec3 position = {0.0f, 0.0f, 0.0f};

	};

	struct DragState {
		// Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
		bool active = false;
		// The position of the mouse at the beginning of the drag action
		vec2 startMouse;
		// The camera state at the beginning of the drag action
		CameraState startCameraState;

		// Constant settings
		float sensitivity = 0.01f;
		float scrollSensitivity = 0.1f;

		// Inertia
		vec2 velocity = { 0.0, 0.0 };
		vec2 previousDelta;
		float intertia = 0.9f;
	};

	// Window and Device
	GLFWwindow* m_window = nullptr;
	WGPUInstance m_instance = nullptr;
	WGPUSurface m_surface = nullptr;
	WGPUDevice m_device = nullptr;
	WGPUQueue m_queue = nullptr;
	WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_Undefined;
	// Keep the error callback alive
	std::unique_ptr<WGPUErrorCallback> m_errorCallbackHandle;

	// Swap Chain
	WGPUSwapChain m_swapChain = nullptr;

	// Depth Buffer
	WGPUTextureFormat m_depthTextureFormat = WGPUTextureFormat_Depth24Plus;
	WGPUTexture m_depthTexture = nullptr;
	WGPUTextureView m_depthTextureView = nullptr;

	// std::unordered_map<const std::string&, WGPUShaderModule> m_shaderModuleMap;
	// std::unordered_map<const std::string&, WGPURenderPipeline> m_pipelines;

	std::unordered_map<std::string, WGPUShaderModule> m_shaderModuleMap;
	std::unordered_map<std::string, WGPURenderPipeline> m_pipelines;


	// Render Pipeline
	WGPUShaderModule m_shaderModule = nullptr;
	WGPURenderPipeline m_pipeline = nullptr;

	// Texture
	WGPUSampler m_sampler = nullptr;
	WGPUTexture m_texture = nullptr;
	WGPUTextureView m_textureView = nullptr;
	
	// Geometry
	WGPUBuffer m_vertexBuffer = nullptr;
	int m_vertexCount = 0;

	WGPUBuffer m_axesVertexBuffer = nullptr;
	int m_axesVertexCount = 0;

	WGPUBuffer m_arrowVertexBuffer = nullptr;
	int m_arrowVertexCount = 0;

	// Uniforms
	WGPUBuffer m_uniformBuffer = nullptr;
	MyUniforms m_uniforms;
	WGPUBuffer m_lightingUniformBuffer = nullptr;
	LightingUniforms m_lightingUniforms;
	bool m_lightingUniformsChanged = true;

	// Bind Group Layout
	WGPUBindGroupLayout m_bindGroupLayout = nullptr;

	// Bind Group
	WGPUBindGroup m_bindGroup = nullptr;

	CameraState m_cameraState;
	DragState m_drag;

	float m_fovy;
	float m_aspectRatio;
	float m_nearPlane;
	float m_farPlane;

	// Graph objects buffers
	WGPUBuffer m_surfaceVertexBuffer = nullptr;
	int m_surfaceVertexCount = 0;
	WGPUBuffer m_curveVertexBuffer = nullptr;
	int m_curveVertexCount = 0;
	bool m_graphObjectsDirty = true;

	// Visualization toggles
	bool m_showVectorField = false;
	bool m_showParametricCurve = true;
	bool m_showParametricSurface = true;

	// Vector field params
	int m_vfExample = 0;
	float m_vfArrowScale = 0.3f;
	int m_vfResolution = 5;
	float m_vfRangeMin = -3.0f;
	float m_vfRangeMax = 3.0f;

	// Curve params
	int m_curveExample = 0;
	int m_curveSegments = 200;
	float m_curveTMin = 0.0f;
	float m_curveTMax = 25.13f; // ~4 * 2 * pi

	// Surface params
	int m_surfaceExample = 0;
	int m_surfaceResolution = 50;
	float m_surfaceUMin = -3.0f;
	float m_surfaceUMax = 3.0f;
	float m_surfaceVMin = -3.0f;
	float m_surfaceVMax = 3.0f;
};
