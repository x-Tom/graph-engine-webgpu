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
#include "ExpressionParser.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <deque>
#include <vector>
#include <utility>

struct Preset {
	std::string name;
	std::vector<std::string> exprs;
	std::array<float, 6> ranges = {0,0,0,0,0,0};
};


// Forward declare
struct GLFWwindow;

// Generalized R^n -> R^m function definition
struct FunctionDefinition {
	std::string name = "r";
	int inputDim = 1;                    // n: 1, 2, or 3
	int outputDim = 3;                   // m: 1, 2, or 3
	std::string paramNames[3] = {"t","",""};
	std::string exprStrings[3] = {"cos(t)","sin(t)","t/(2*pi)"};
	ExpressionParser parsers[3];
	bool isValid = false;
	std::string errorMsg;
	bool show = true;
	float color[3] = {1.0f, 1.0f, 0.0f};
	float opacity = 1.0f;                // Surface/curve opacity (0=transparent, 1=opaque)
	float rangeMin[3] = {-10.0f, -10.0f, -10.0f};
	float rangeMax[3] = {10.0f, 10.0f, 10.0f};
	int resolution[2] = {200, 200};      // [segments/uRes, vRes]
	float tubeRadius = 0.03f;
	float arrowScale = 0.3f;
	int vfResolution = 5;
	int curvePlane = 0;                  // For R^1->R^2 curves: 0=xy, 1=xz, 2=yz

	// Overlay options
	bool wireframe = false;           // surfaces (n=2) and curves (n=1): render as wireframe/lines
	bool showTangentVectors = false;  // curves (n=1) and surfaces (n=2): tangent arrows
	int surfaceTangentMode = 0;       // For surfaces: 0=both u&v, 1=u only, 2=v only
	bool showNormalVectors = false;   // surfaces (n=2) and curves (n=1): normal arrows
	bool flipNormalVectors = false;   // surfaces and curves: flip normal direction
	bool showFrenetFrame = false;     // curves only (n=1): T/N/B trihedron
	float frenetT = 0.5f;            // parameter value for Frenet frame (normalized 0-1)
	bool showGradientField = false;   // scalar fields (m=1): gradient arrows
	bool showVectorField = false;     // vector fields (n=3, m>=2): show field arrows
	bool showStreamlines = false;     // vector fields (n=3, m>=2): show streamlines
	int overlayVectorCount = 10;      // how many tangent/normal arrows to show
	float overlayVectorScale = 0.3f;  // scale of overlay arrows
};

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

	// Compile all expressions in a FunctionDefinition
	void compileFunctionDef(FunctionDefinition& fd);

	// Axes
	bool initAxesGeometry();
	bool initXYPlaneWireframeGeometry(float spacing, float len = 1000);
	bool initPrincipalPlanesWireframeGeometry(float spacing, float len = 1000);
	bool initWireframeGeometry(float spacing, float len = 500);
	void terminateAxesGeometry();
	void rebuildAxesBuffer();

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
	void loadPresets();

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

	// Deferred buffer destruction (wait N frames before destroying)
	static constexpr int BUFFER_RELEASE_DELAY = 5;  // Increased from 3 to reduce crashes
	std::vector<std::pair<WGPUBuffer, int>> m_pendingBufferReleases;
	int m_framesSinceLastUpdate = 0;  // Throttle geometry updates
	void deferBufferRelease(WGPUBuffer buffer);
	void processPendingReleases();

	// Boat visibility
	bool m_showBoat = false;

	// Function definitions (generalized R^n -> R^m)
	std::deque<FunctionDefinition> m_functions;

	// Axes cosmetic options
	bool m_showAxes = true;
	bool m_showGrid = true;
	float m_gridSpacing = 1.0f;
	float m_gridExtent = 20.0f;
	bool m_showXYGrid = true;
	bool m_showXZGrid = false;
	bool m_showYZGrid = false;
	float m_gridColor[3] = {1.0f, 1.0f, 1.0f};
	float m_axisColors[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
	float m_bgColor[3] = {0.6f, 0.6f, 0.7f};
	bool m_axesDirty = false;
	bool m_needsResize = false;

	// Presets loaded from resources/presets.json, keyed by "n_m"
	std::unordered_map<std::string, std::vector<Preset>> m_presets;
};
