#include "Application.h"
#include "ResourceManager.h"
#include "GraphObjects.h"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/polar_coordinates.hpp>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <misc/cpp/imgui_stdlib.h>

#include "tinyexpr/tinyexpr.h"

#include <iostream>
#include <cassert>
#include <filesystem>
#include <sstream>
#include <array>
#include <cmath>
#include <map>

using VertexAttributes = ResourceManager::VertexAttributes;

constexpr float PI = 3.14159265358979323846f;

// Custom ImGui widgets
namespace ImGui {
	bool DragDirection(const char* label, glm::vec4& direction) {
		glm::vec2 angles = glm::degrees(glm::polar(glm::vec3(direction)));
		bool changed = ImGui::DragFloat2(label, glm::value_ptr(angles));
		direction = glm::vec4(glm::euclidean(glm::radians(angles)), direction.w);
		return changed;
	}

	// InputFloat with expression parsing (supports "pi", "2*pi", "sqrt(2)", etc.)
	bool InputFloatExpr(const char* label, float* v, const char* format = "%.3f", ImGuiInputTextFlags flags = 0) {
		// Create a unique ID for storing the text buffer
		static std::map<ImGuiID, std::string> textBuffers;
		ImGuiID id = ImGui::GetID(label);

		// Initialize buffer if needed
		if (textBuffers.find(id) == textBuffers.end()) {
			char buf[64];
			snprintf(buf, sizeof(buf), format, *v);
			textBuffers[id] = buf;
		}

		// Show input text
		bool changed = false;
		if (ImGui::InputText(label, &textBuffers[id], flags | ImGuiInputTextFlags_EnterReturnsTrue)) {
			// Try to parse as expression
			int error;
			double result = te_interp(textBuffers[id].c_str(), &error);
			if (error == 0) {
				*v = static_cast<float>(result);
				changed = true;
				// Update buffer to show evaluated value
				char buf[64];
				snprintf(buf, sizeof(buf), format, *v);
				textBuffers[id] = buf;
			}
		}

		// Also update if focus is lost and value changed
		if (!ImGui::IsItemActive() && !ImGui::IsItemFocused()) {
			int error;
			double result = te_interp(textBuffers[id].c_str(), &error);
			if (error == 0 && std::abs(static_cast<float>(result) - *v) > 1e-6f) {
				*v = static_cast<float>(result);
				changed = true;
			}
			// Always sync buffer to current value when not focused
			char buf[64];
			snprintf(buf, sizeof(buf), format, *v);
			textBuffers[id] = buf;
		}

		return changed;
	}

	// Unified drag + expression widget (ctrl+click to type expressions, or drag to adjust)
	bool DragFloatExpr(const char* label, float* v, float speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f) {
		static std::map<ImGuiID, std::string> editBuffers;
		static std::map<ImGuiID, bool> isEditingText;
		ImGuiID id = ImGui::GetID(label);

		bool changed = false;
		ImGui::SetNextItemWidth(-1);

		// Check if we should enter text edit mode (ctrl+click was detected last frame)
		bool wasEditing = isEditingText[id];
		bool shouldEdit = wasEditing;

		// Use DragFloat for normal dragging behavior
		if (!shouldEdit) {
			changed = ImGui::DragFloat(label, v, speed, v_min, v_max, "%.3f");

			// Detect ctrl+click to enter edit mode
			if (ImGui::IsItemActive() && ImGui::IsItemClicked() && ImGui::GetIO().KeyCtrl) {
				isEditingText[id] = true;
				char buf[64];
				snprintf(buf, sizeof(buf), "%.3f", *v);
				editBuffers[id] = buf;
			}
		} else {
			// Text editing mode with expression parsing
			ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;

			if (ImGui::InputText(label, &editBuffers[id], flags)) {
				// User pressed Enter - parse as expression
				int error;
				double result = te_interp(editBuffers[id].c_str(), &error);
				if (error == 0) {
					*v = static_cast<float>(result);
					changed = true;
				}
				isEditingText[id] = false;
			}

			// Exit edit mode if focus lost
			if (!ImGui::IsItemActive() && !ImGui::IsItemFocused()) {
				// Try to parse on focus loss too
				int error;
				double result = te_interp(editBuffers[id].c_str(), &error);
				if (error == 0) {
					*v = static_cast<float>(result);
					changed = true;
				}
				isEditingText[id] = false;
			}
		}

		return changed;
	}
} // namespace ImGui

///////////////////////////////////////////////////////////////////////////////
// Public methods

bool Application::onInit() {
	if (!initWindowAndDevice()) return false;
	if (!initSwapChain()) return false;
	if (!initDepthBuffer()) return false;
	if (!initBindGroupLayout()) return false;
	// if (!initRenderPipeline()) return false;

	if (!initRenderPipeline("boat", RESOURCE_DIR "/shader.wgsl", WGPUPrimitiveTopology_TriangleList)) return false;
	if (!initRenderPipeline("axes", RESOURCE_DIR "/axes.wgsl", WGPUPrimitiveTopology_LineList)) return false;
	if (!initRenderPipeline("surface", RESOURCE_DIR "/surface.wgsl", WGPUPrimitiveTopology_TriangleList)) return false;


	if (!initTexture()) return false;
	// std::cout << "Passed [1]" << std::endl;
	if (!initGeometry()) return false;
	// if (!initAxesGeometry()) return false;
	rebuildAxesBuffer();
	// if (!initPrincipalPlanesWireframeGeometry(1)) return false;
	// if (!initWireframeGeometry(5)) return false;
	// std::cout << "Passed [2]" << std::endl;

	// m_fovy = glm::radians(180.0f);
    // m_aspectRatio = 640.0f / 480.0f;
    // m_nearPlane = 0.01f;
    // m_farPlane = 100.0f; 
	// if (!initUniforms()) return false;
	if (!initUniforms(glm::radians(90.0f), 640.0f / 480.0f, 0.01f, 100.0f)) return false;

	// std::cout << "Passed [3]" << std::endl;
	if (!initLightingUniforms()) return false;
	// std::cout << "Passed [4]" << std::endl;
	if (!initBindGroup()) return false;
	if (!initGraphObjects()) return false;
	if (!initGui()) return false;
	return true;
}

void Application::deferBufferRelease(WGPUBuffer buffer) {
	if (buffer) {
		m_pendingBufferReleases.push_back({buffer, BUFFER_RELEASE_DELAY});
	}
}

void Application::processPendingReleases() {
	for (auto it = m_pendingBufferReleases.begin(); it != m_pendingBufferReleases.end(); ) {
		it->second--;
		if (it->second <= 0) {
			wgpuBufferDestroy(it->first);
			wgpuBufferRelease(it->first);
			it = m_pendingBufferReleases.erase(it);
		} else {
			++it;
		}
	}
}

void Application::onFrame() {
	glfwPollEvents();
	processPendingReleases();
	updateDragInertia();
	updateLightingUniforms();
	updateGraphObjects();

	// Update uniform buffer
	m_uniforms.time = static_cast<float>(glfwGetTime());
	// m_queue.writeBuffer(m_uniformBuffer, offsetof(MyUniforms, time), &m_uniforms.time, sizeof(MyUniforms::time));
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, offsetof(MyUniforms, time), &m_uniforms.time, sizeof(MyUniforms::time));


	// WGPU TextureView nextTexture = m_swapChain.getCurrentTextureView();
	WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(m_swapChain);


	if (!nextTexture) {
		std::cerr << "Cannot acquire next swap chain texture" << std::endl;
		return;
	}

	WGPUCommandEncoderDescriptor commandEncoderDesc = {};
	commandEncoderDesc.label = "Command Encoder";
	// WGPUCommandEncoder encoder = m_device.createCommandEncoder(commandEncoderDesc);
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &commandEncoderDesc);


	WGPURenderPassDescriptor renderPassDesc{};

	WGPURenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = nextTexture;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
	renderPassColorAttachment.clearValue = WGPUColor{m_bgColor[0], m_bgColor[1], m_bgColor[2], 1.0};
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;

	WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
	depthStencilAttachment.view = m_depthTextureView;
	depthStencilAttachment.depthClearValue = 1.0f;
	depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
	depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
	depthStencilAttachment.depthReadOnly = false;
	depthStencilAttachment.stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
	// depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
	// depthStencilAttachment.stencilStoreOp = StoreOp::Store;
	depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
	depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
#else
	depthStencilAttachment.stencilLoadOp = WGPULoadOp_Undefined;
	depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
#endif
	depthStencilAttachment.stencilReadOnly = true;

	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

	renderPassDesc.timestampWriteCount = 0;
	renderPassDesc.timestampWrites = nullptr;
	WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
	
	if(!renderPass) std::cout << "renderpass command encoder failed" << std::endl;

	if (m_showBoat) {
		m_pipeline = m_pipelines["boat"];
		wgpuRenderPassEncoderSetPipeline(renderPass, m_pipeline);
		wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_vertexBuffer, 0, m_vertexCount * sizeof(VertexAttributes));
		wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_bindGroup, 0, nullptr);
		wgpuRenderPassEncoderDraw(renderPass, m_vertexCount, 1, 0, 0);
	}

	if (m_showAxes && m_axesVertexCount > 0 && m_axesVertexBuffer) {
		m_pipeline = m_pipelines["axes"];
		wgpuRenderPassEncoderSetPipeline(renderPass, m_pipeline);
		wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_axesVertexBuffer, 0, m_axesVertexCount * sizeof(VertexAttributes));
		wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_bindGroup, 0, nullptr);
		wgpuRenderPassEncoderDraw(renderPass, m_axesVertexCount, 1, 0, 0);
	}

	// Draw surfaces + arrows (TriangleList, "surface" pipeline)
	if (m_surfaceVertexCount > 0 && m_surfaceVertexBuffer) {
		wgpuRenderPassEncoderSetPipeline(renderPass, m_pipelines["surface"]);
		wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_surfaceVertexBuffer, 0, m_surfaceVertexCount * sizeof(VertexAttributes));
		wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_bindGroup, 0, nullptr);
		wgpuRenderPassEncoderDraw(renderPass, m_surfaceVertexCount, 1, 0, 0);
	}

	// Wireframe overlay lines (LineList, "axes" pipeline)
	if (m_curveVertexCount > 0 && m_curveVertexBuffer) {
		wgpuRenderPassEncoderSetPipeline(renderPass, m_pipelines["axes"]);
		wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_curveVertexBuffer, 0, m_curveVertexCount * sizeof(VertexAttributes));
		wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_bindGroup, 0, nullptr);
		wgpuRenderPassEncoderDraw(renderPass, m_curveVertexCount, 1, 0, 0);
	}

	// We add the GUI drawing commands to the render pass
	updateGui(renderPass);

	wgpuRenderPassEncoderEnd(renderPass);
	wgpuRenderPassEncoderRelease(renderPass);
	

	wgpuTextureViewRelease(nextTexture);


	WGPUCommandBufferDescriptor cmdBufferDescriptor{};
	cmdBufferDescriptor.label = "Command buffer";
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
	wgpuCommandEncoderRelease(encoder);
	wgpuQueueSubmit(m_queue, 1, &command);
	
	
	wgpuCommandBufferRelease(command);

	wgpuSwapChainPresent(m_swapChain);
	
	

#ifdef WEBGPU_BACKEND_DAWN
	// Check for pending error callbacks
	wgpuDeviceTick(m_device);

#endif
}

void Application::onFinish() {
	terminateGui();
	terminateGraphObjects();
	terminateBindGroup();
	terminateLightingUniforms();
	terminateUniforms();
	terminateGeometry();
	terminateTexture();
	terminateRenderPipelines();
	terminateBindGroupLayout();
	terminateDepthBuffer();
	terminateSwapChain();
	terminateWindowAndDevice();
}

bool Application::isRunning() {
	return !glfwWindowShouldClose(m_window);
}

void Application::onResize() {
	// Terminate in reverse order
	terminateDepthBuffer();
	terminateSwapChain();

	// Re-init
	initSwapChain();
	initDepthBuffer();

	updateProjectionMatrix();
}

void Application::onMouseMove(double xpos, double ypos) {
	if (m_drag.active) {
		vec2 currentMouse = vec2(-(float)xpos, (float)ypos);
		vec2 delta = (currentMouse - m_drag.startMouse) * m_drag.sensitivity;
		m_cameraState.angles = m_drag.startCameraState.angles + delta;
		// Clamp to avoid going too far when orbitting up/down
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
		updateViewMatrix();

		// Inertia
		m_drag.velocity = delta - m_drag.previousDelta;
		m_drag.previousDelta = delta;
	}
}

void Application::onMouseButton(int button, int action, int /* modifiers */) {
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		// Don't rotate the camera if the mouse is already captured by an ImGui
		// interaction at this frame.
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		switch (action) {
		case GLFW_PRESS:
			m_drag.active = true;
			double xpos, ypos;
			glfwGetCursorPos(m_window, &xpos, &ypos);
			m_drag.startMouse = vec2(-(float)xpos, (float)ypos);
			m_drag.startCameraState = m_cameraState;
			break;
		case GLFW_RELEASE:
			m_drag.active = false;
			break;
		}
	}
}

// void Application::onKey(int key, int action) {
//     if (action == GLFW_PRESS || action == GLFW_REPEAT) {
//         glm::vec3 forward = glm::normalize(glm::vec3(
//             cos(m_cameraState.angles.x) * cos(m_cameraState.angles.y),
//             sin(m_cameraState.angles.y),
//             sin(m_cameraState.angles.x) * cos(m_cameraState.angles.y)
//         ));

//         glm::vec3 right = glm::normalize(glm::vec3(
//             cos(m_cameraState.angles.x - glm::half_pi<float>()),
//             0,
//             sin(m_cameraState.angles.x - glm::half_pi<float>())
//         ));

//         switch (key) {
//             case GLFW_KEY_W:
//                 m_cameraState.position += forward * m_cameraState.movementSpeed;
//                 break;
//             case GLFW_KEY_S:
//                 m_cameraState.position -= forward * m_cameraState.movementSpeed;
//                 break;
//             case GLFW_KEY_A:
//                 m_cameraState.position -= right * m_cameraState.movementSpeed;
//                 break;
//             case GLFW_KEY_D:
//                 m_cameraState.position += right * m_cameraState.movementSpeed;
//                 break;
//         }
//         updateViewMatrix();
//     }
// }


void Application::onScroll(double /* xoffset */, double yoffset) {
	m_cameraState.zoom += m_drag.scrollSensitivity * static_cast<float>(yoffset);
	m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -5.0f, 2.0f);
	updateViewMatrix();
}

///////////////////////////////////////////////////////////////////////////////
// Private methods

bool Application::initWindowAndDevice() {
	WGPUInstanceDescriptor instanceDesc = {};
	m_instance = wgpuCreateInstance(&instanceDesc);
	if (!m_instance) {
		std::cerr << "Could not initialize WebGPU!" << std::endl;
		return false;
	}

	if (!glfwInit()) {
		std::cerr << "Could not initialize GLFW!" << std::endl;
		return false;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(640, 480, "WEBGPU", NULL, NULL);
	if (!m_window) {
		std::cerr << "Could not open window!" << std::endl;
		return false;
	}

	std::cout << "Requesting adapter..." << std::endl;
	m_surface = glfwGetWGPUSurface(m_instance, m_window);
	WGPURequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = m_surface;
	// Adapter adapter = m_instance.requestAdapter(adapterOpts);
	WGPUAdapter adapter = requestAdapter(m_instance, &adapterOpts);
	// WGPUAdapter adapter = wgpuInstanceRequestAdapter(m_instance, &adapterOpts);

	std::cout << "Got adapter: " << adapter << std::endl;

	// SupportedLimits supportedLimits;
	WGPUSupportedLimits supportedLimits = {};
	wgpuAdapterGetLimits(adapter, &supportedLimits);
	m_supported_limits = supportedLimits;
	// adapter.getLimits(&supportedLimits);



	std::cout << "Requesting device..." << std::endl;
	WGPURequiredLimits requiredLimits = {};

	int maxbufsize = 90000000;

	requiredLimits.limits.maxVertexAttributes = 4;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBufferSize = maxbufsize * sizeof(VertexAttributes);
	requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	// requiredLimits.limits.minStorageBufferOffsetAlignment = 256;

	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	// requiredLimits.limits.minUniformBufferOffsetAlignment = 256;

	requiredLimits.limits.maxInterStageShaderComponents = 11;
	//                                                    ^ This was 8
	requiredLimits.limits.maxBindGroups = 2;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 2;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 1;
	requiredLimits.limits.maxSamplersPerShaderStage = 1;

	// Always use adapter's texture resulution limits.
	requiredLimits.limits.maxTextureDimension1D = supportedLimits.limits.maxTextureDimension1D;
	requiredLimits.limits.maxTextureDimension2D = supportedLimits.limits.maxTextureDimension2D;
	// requiredLimits.limits.maxTextureDimension3D = supportedLimits.limits.maxTextureDimension3D;
	requiredLimits.limits.maxTextureDimension3D = 2048;

	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeaturesCount = 0;
	deviceDesc.requiredLimits = &requiredLimits;
	deviceDesc.defaultQueue.label = "The default queue";

	m_current_limits = requiredLimits;


	// std::cout << "Requesting Device 2" << std::endl; 

	m_device = requestDevice(adapter, &deviceDesc);
	// m_device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << m_device << std::endl;

	// Add an error callback for more debug info

	// auto callbk = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
    // 	std::cout << "Device error: type " << type;
	// 	if (message) std::cout << " (message: " << message << ")";
	// 	std::cout << std::endl;
	// };

	// m_errorCallbackHandle = std::unique_ptr<>()

	wgpuDeviceSetUncapturedErrorCallback(m_device, [](WGPUErrorType type, char const* message, void*) {
	// m_errorCallbackHandle = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
		std::cout << "Device error: type " << type;
		if (message) std::cout << " (message: " << message << ")";
		std::cout << std::endl;
	}, nullptr);

	if (m_device == nullptr) {
		std::cerr << "Failed to get device." << std::endl;
		return 1;
	}



	// m_queue = m_device.getQueue();
	m_queue = wgpuDeviceGetQueue(m_device);

	// m_swapChainFormat = wgpuSurfaceGetPreferredFormat(m_surface, adapter);


#ifdef WEBGPU_BACKEND_WGPU
	// m_swapChainFormat = m_surface.getPreferredFormat(adapter);
	m_swapChainFormat = wgpuSurfaceGetPreferredFormat(m_surface, adapter);
#else
	// m_swapChainFormat = TextureFormat::BGRA8Unorm;
	m_swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
#endif

	// Add window callbacks
	// Set the user pointer to be "this"
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int, int) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onResize();
	});
	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseMove(xpos, ypos);
	});
	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseButton(button, action, mods);
	});
	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onScroll(xoffset, yoffset);
	});

	// adapter.release();
	wgpuAdapterRelease(adapter);
	return m_device != nullptr;
}

void Application::terminateWindowAndDevice() {

	wgpuQueueRelease(m_queue);
	wgpuDeviceRelease(m_device);
	wgpuSurfaceRelease(m_surface);
	wgpuInstanceRelease(m_instance);

	// m_queue.release();
	// m_device.release();
	// m_surface.release();
	// m_instance.release();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}


bool Application::initSwapChain() {
	// Get the current size of the window's framebuffer:
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	std::cout << "Creating swapchain..." << std::endl;
	WGPUSwapChainDescriptor swapChainDesc = {};
	swapChainDesc.width = static_cast<uint32_t>(width);
	swapChainDesc.height = static_cast<uint32_t>(height);
	// swapChainDesc.usage = TextureUsage::RenderAttachment;
	swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
	swapChainDesc.format = m_swapChainFormat;
	swapChainDesc.presentMode = WGPUPresentMode_Fifo;
	// swapChainDesc.presentMode = PresentMode::Fifo;

	// m_swapChain = m_device.createSwapChain(m_surface, swapChainDesc);
	m_swapChain = wgpuDeviceCreateSwapChain(m_device, m_surface, &swapChainDesc);

	std::cout << "Swapchain: " << m_swapChain << std::endl;
	return m_swapChain != nullptr;
}

void Application::terminateSwapChain() {
	// m_swapChain.release();
	wgpuSwapChainRelease(m_swapChain);
}


bool Application::initDepthBuffer() {
	// Get the current size of the window's framebuffer:
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	// Create the depth texture
	WGPUTextureDescriptor depthTextureDesc = {};
	depthTextureDesc.dimension = WGPUTextureDimension_2D;
	depthTextureDesc.format = m_depthTextureFormat;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthTextureFormat;
	// m_depthTexture = m_device.createTexture(depthTextureDesc);
	m_depthTexture = wgpuDeviceCreateTexture(m_device, &depthTextureDesc);
	std::cout << "Depth texture: " << m_depthTexture << std::endl;

	// Create the view of the depth texture manipulated by the rasterizer
	WGPUTextureViewDescriptor depthTextureViewDesc = {};
	depthTextureViewDesc.aspect = WGPUTextureAspect_DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = WGPUTextureViewDimension_2D;
	depthTextureViewDesc.format = m_depthTextureFormat;
	// m_depthTextureView = m_depthTexture.createView(depthTextureViewDesc);
	m_depthTextureView = wgpuTextureCreateView(m_depthTexture, &depthTextureViewDesc);
	std::cout << "Depth texture view: " << m_depthTextureView << std::endl;

	return m_depthTextureView != nullptr;
}

void Application::terminateDepthBuffer() {
	// m_depthTextureView.release();
	// m_depthTexture.destroy();
	// m_depthTexture.release();
	
	wgpuTextureViewRelease(m_depthTextureView);
	wgpuTextureDestroy(m_depthTexture);
	wgpuTextureRelease(m_depthTexture);
}


// bool Application::initRenderPipeline() {
// 	std::cout << "Creating shader module..." << std::endl;
// 	m_shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", m_device);
// 	std::cout << "Shader module: " << m_shaderModule << std::endl;

// 	std::cout << "Creating render pipeline..." << std::endl;
// 	WGPURenderPipelineDescriptor pipelineDesc = {};

// 	// Vertex fetch
// 	std::vector<WGPUVertexAttribute> vertexAttribs(4);

// 	// Position attribute
// 	vertexAttribs[0].shaderLocation = 0;
// 	vertexAttribs[0].format = WGPUVertexFormat_Float32x3;
// 	vertexAttribs[0].offset = 0;

// 	// Normal attribute
// 	vertexAttribs[1].shaderLocation = 1;
// 	vertexAttribs[1].format = WGPUVertexFormat_Float32x3;
// 	vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

// 	// Color attribute
// 	vertexAttribs[2].shaderLocation = 2;
// 	vertexAttribs[2].format = WGPUVertexFormat_Float32x3;
// 	vertexAttribs[2].offset = offsetof(VertexAttributes, color);

// 	// UV attribute
// 	vertexAttribs[3].shaderLocation = 3;
// 	vertexAttribs[3].format = WGPUVertexFormat_Float32x2;
// 	vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

// 	WGPUVertexBufferLayout vertexBufferLayout = {};
// 	vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
// 	vertexBufferLayout.attributes = vertexAttribs.data();
// 	vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
// 	vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

// 	pipelineDesc.vertex.bufferCount = 1;
// 	pipelineDesc.vertex.buffers = &vertexBufferLayout;

// 	pipelineDesc.vertex.module = m_shaderModule;
// 	pipelineDesc.vertex.entryPoint = "vs_main";
// 	pipelineDesc.vertex.constantCount = 0;
// 	pipelineDesc.vertex.constants = nullptr;

// 	pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
// 	pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
// 	pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
// 	pipelineDesc.primitive.cullMode = WGPUCullMode_None;

// 	WGPUFragmentState fragmentState = {};
// 	pipelineDesc.fragment = &fragmentState;
// 	fragmentState.module = m_shaderModule;
// 	fragmentState.entryPoint = "fs_main";
// 	fragmentState.constantCount = 0;
// 	fragmentState.constants = nullptr;

// 	WGPUBlendState blendState = {};
// 	blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
// 	blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
// 	blendState.color.operation = WGPUBlendOperation_Add;
// 	blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
// 	blendState.alpha.dstFactor = WGPUBlendFactor_One;
// 	blendState.alpha.operation = WGPUBlendOperation_Add;

// 	WGPUColorTargetState colorTarget = {};
// 	colorTarget.format = m_swapChainFormat;
// 	colorTarget.blend = &blendState;
// 	colorTarget.writeMask = WGPUColorWriteMask_All;

// 	fragmentState.targetCount = 1;
// 	fragmentState.targets = &colorTarget;


// 	WGPUStencilFaceState noOpStencilFaceState = {};
// 	noOpStencilFaceState.compare = WGPUCompareFunction_Always;
// 	noOpStencilFaceState.failOp = WGPUStencilOperation_Keep;
// 	noOpStencilFaceState.depthFailOp = WGPUStencilOperation_Keep;
// 	noOpStencilFaceState.passOp = WGPUStencilOperation_Keep;

// 	WGPUDepthStencilState depthStencilState = {};
// 	depthStencilState.depthCompare = WGPUCompareFunction_Less;
// 	depthStencilState.depthWriteEnabled = true;
// 	depthStencilState.format = m_depthTextureFormat;
// 	depthStencilState.stencilReadMask = 0;
// 	depthStencilState.stencilWriteMask = 0;
// 	depthStencilState.stencilFront = noOpStencilFaceState;
// 	depthStencilState.stencilBack = noOpStencilFaceState;


// 	pipelineDesc.depthStencil = &depthStencilState;

// 	pipelineDesc.multisample.count = 1;
// 	pipelineDesc.multisample.mask = ~0u;
// 	pipelineDesc.multisample.alphaToCoverageEnabled = false;

// 	// Create the pipeline layout
// 	WGPUPipelineLayoutDescriptor layoutDesc{};
// 	layoutDesc.bindGroupLayoutCount = 1;
// 	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindGroupLayout;
// 	WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &layoutDesc);
// 	pipelineDesc.layout = layout;

// 	m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);
// 	std::cout << "Render pipeline: " << m_pipeline << std::endl;

// 	wgpuPipelineLayoutRelease(layout);
// 	return m_pipeline != nullptr;
// }

bool Application::initRenderPipeline(const std::string& pipelineName, const std::string& shaderFileName, WGPUPrimitiveTopology topology) {
	std::cout << "Creating shader module..." << std::endl;
	// m_shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", m_device);
	m_shaderModule = ResourceManager::loadShaderModule(shaderFileName, m_device);
	std::cout << "Shader module: " << m_shaderModule << std::endl;

	m_shaderModuleMap[pipelineName] = m_shaderModule;

	std::cout << "Creating render pipeline..." << std::endl;
	WGPURenderPipelineDescriptor pipelineDesc = {};

	// Vertex fetch
	std::vector<WGPUVertexAttribute> vertexAttribs(4);

	// Position attribute
	vertexAttribs[0].shaderLocation = 0;
	vertexAttribs[0].format = WGPUVertexFormat_Float32x3;
	vertexAttribs[0].offset = 0;

	// Normal attribute
	vertexAttribs[1].shaderLocation = 1;
	vertexAttribs[1].format = WGPUVertexFormat_Float32x3;
	vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

	// Color attribute
	vertexAttribs[2].shaderLocation = 2;
	vertexAttribs[2].format = WGPUVertexFormat_Float32x3;
	vertexAttribs[2].offset = offsetof(VertexAttributes, color);

	// UV attribute
	vertexAttribs[3].shaderLocation = 3;
	vertexAttribs[3].format = WGPUVertexFormat_Float32x2;
	vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

	WGPUVertexBufferLayout vertexBufferLayout = {};
	vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
	vertexBufferLayout.attributes = vertexAttribs.data();
	vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
	vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;

	pipelineDesc.vertex.module = m_shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	// pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	pipelineDesc.primitive.topology = topology;
	pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
	pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
	pipelineDesc.primitive.cullMode = WGPUCullMode_None;

	WGPUFragmentState fragmentState = {};
	pipelineDesc.fragment = &fragmentState;
	fragmentState.module = m_shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

	WGPUBlendState blendState = {};
	blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
	blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	blendState.color.operation = WGPUBlendOperation_Add;
	blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
	blendState.alpha.dstFactor = WGPUBlendFactor_One;
	blendState.alpha.operation = WGPUBlendOperation_Add;

	WGPUColorTargetState colorTarget = {};
	colorTarget.format = m_swapChainFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = WGPUColorWriteMask_All;

	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;


	WGPUStencilFaceState noOpStencilFaceState = {};
	noOpStencilFaceState.compare = WGPUCompareFunction_Always;
	noOpStencilFaceState.failOp = WGPUStencilOperation_Keep;
	noOpStencilFaceState.depthFailOp = WGPUStencilOperation_Keep;
	noOpStencilFaceState.passOp = WGPUStencilOperation_Keep;

	WGPUDepthStencilState depthStencilState = {};
	depthStencilState.depthCompare = WGPUCompareFunction_Less;
	depthStencilState.depthWriteEnabled = true;
	depthStencilState.format = m_depthTextureFormat;
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;
	depthStencilState.stencilFront = noOpStencilFaceState;
	depthStencilState.stencilBack = noOpStencilFaceState;


	pipelineDesc.depthStencil = &depthStencilState;

	pipelineDesc.multisample.count = 1;
	pipelineDesc.multisample.mask = ~0u;
	pipelineDesc.multisample.alphaToCoverageEnabled = false;

	// Create the pipeline layout
	WGPUPipelineLayoutDescriptor layoutDesc{};
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindGroupLayout;
	WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &layoutDesc);
	pipelineDesc.layout = layout;

	m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);
	std::cout << "Render pipeline: " << m_pipeline << std::endl;

	m_pipelines[pipelineName] = m_pipeline;

	wgpuPipelineLayoutRelease(layout);
	return m_pipeline != nullptr;
}

void Application::terminateRenderPipeline(const std::string& pipelineName) {
	// m_pipeline.release();
	// m_shaderModule.release();
	wgpuRenderPipelineRelease(m_pipelines[pipelineName]);
	wgpuShaderModuleRelease(m_shaderModuleMap[pipelineName]);
	// auto pipelineIt = m_pipelines.find(pipelineName);
    // if (pipelineIt != m_pipelines.end()) {
    //     wgpuRenderPipelineRelease(pipelineIt->second);
    //     m_pipelines.erase(pipelineIt);
    // }

    // // Check and release the shader module if it exists
    // auto shaderModuleIt = m_shaderModuleMap.find(pipelineName);
    // if (shaderModuleIt != m_shaderModuleMap.end()) {
    //     wgpuShaderModuleRelease(shaderModuleIt->second);
    //     m_shaderModuleMap.erase(shaderModuleIt);
    // }
}

void Application::terminateRenderPipelines() {
    for (const auto& pair : m_pipelines) {
        terminateRenderPipeline(pair.first);
    }
    m_pipelines.clear();
    m_shaderModuleMap.clear();
}



bool Application::initTexture() {
	// Create a sampler
	WGPUSamplerDescriptor samplerDesc = {};
	samplerDesc.addressModeU = WGPUAddressMode_Repeat;
	samplerDesc.addressModeV = WGPUAddressMode_Repeat;
	samplerDesc.addressModeW = WGPUAddressMode_Repeat;
	samplerDesc.magFilter = WGPUFilterMode_Linear;
	samplerDesc.minFilter = WGPUFilterMode_Linear;
	samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
	samplerDesc.lodMinClamp = 0.0f;
	samplerDesc.lodMaxClamp = 8.0f;
	samplerDesc.compare = WGPUCompareFunction_Undefined;
	samplerDesc.maxAnisotropy = 1;
	m_sampler = wgpuDeviceCreateSampler(m_device, &samplerDesc);

	// Create a texture
	m_texture = ResourceManager::loadTexture(RESOURCE_DIR "/fourareen2K_albedo.jpg", m_device, &m_textureView);
	if (!m_texture) {
		std::cerr << "Could not load texture!" << std::endl;
		return false;
	}
	std::cout << "Texture: " << m_texture << std::endl;
	std::cout << "Texture view: " << m_textureView << std::endl;

	return m_textureView != nullptr;
}

void Application::terminateTexture() {
	// m_textureView.release();
	// m_texture.destroy();
	// m_texture.release();
	// m_sampler.release();
	wgpuTextureViewRelease(m_textureView);
	wgpuTextureDestroy(m_texture);
	wgpuTextureRelease(m_texture);
	wgpuSamplerRelease(m_sampler);
}


bool Application::initGeometry() {
	// Load mesh data from OBJ file
	std::vector<VertexAttributes> vertexData;
	bool success = ResourceManager::loadGeometryFromObj(RESOURCE_DIR "/fourareen.obj", vertexData);
	if (!success) {
		std::cerr << "Could not load geometry!" << std::endl;
		return false;
	}

	// Create vertex buffer
	WGPUBufferDescriptor bufferDesc = {};
	bufferDesc.size = vertexData.size() * sizeof(VertexAttributes);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
	bufferDesc.mappedAtCreation = false;
	m_vertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	wgpuQueueWriteBuffer(m_queue, m_vertexBuffer, 0, vertexData.data(), bufferDesc.size);
	m_vertexCount = static_cast<int>(vertexData.size());

	return m_vertexBuffer != nullptr;
}

bool Application::initAxesGeometry() {
	
	std::vector<VertexAttributes> axesVertexData = {
    // X axis (red)
		{ {-1000.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
		{ { 1000.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },

		// Y axis (green)
		{ {0.0f, -1000.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },
		{ {0.0f,  1000.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },

		// Z axis (blue)
		{ {0.0f, 0.0f, -1000.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {0.0f, 0.0f,  1000.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
	};


	WGPUBufferDescriptor bufferDesc = {};
	bufferDesc.size = axesVertexData.size() * sizeof(VertexAttributes);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
	bufferDesc.mappedAtCreation = false;
	m_axesVertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	wgpuQueueWriteBuffer(m_queue, m_axesVertexBuffer, 0, axesVertexData.data(), bufferDesc.size);
	m_axesVertexCount = static_cast<int>(axesVertexData.size());

	return m_axesVertexBuffer != nullptr;
}

bool Application::initXYPlaneWireframeGeometry(float spacing, float len) {
	
	std::vector<VertexAttributes> axesVertexData = {
    	// X axis (red)
		{ {-len, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
		{ { len, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },

		// Y axis (green)
		{ {0.0f, -len, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },
		{ {0.0f,  len, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },

		// Z axis (blue)
		{ {0.0f, 0.0f, -len}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {0.0f, 0.0f,  len}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
	};



	for (float i = -len; i <= len; i += spacing) {

		axesVertexData.insert(axesVertexData.end(), {
			
			// --- XY Plane ---

			// X axis wireframe
			{ {-len, i, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			{ { len, i, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },

			// Y axis wireframe
			{ {i, -len, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			{ {i, len, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			
			// // --- XZ Plane ---

			// // X axis wireframe
			// { {-len, 0.0f, i}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			// { { len, 0.0f, i}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },

			// // Z axis wireframe
			// { {i, 0.0f, -len}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			// { {i, 0.0f, len}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			
			// // --- YZ Plane ---
			
			// // Y axis wireframe
			// { {0.0f, -len, i}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			// { {0.0f, len, i}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },

			// // Z axis wireframe
			// { {0.0f, i, -len}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			// { {0.0f, i, len}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		});

	}

	WGPUBufferDescriptor bufferDesc = {};
	bufferDesc.size = axesVertexData.size() * sizeof(VertexAttributes);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
	bufferDesc.mappedAtCreation = false;
	m_axesVertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	wgpuQueueWriteBuffer(m_queue, m_axesVertexBuffer, 0, axesVertexData.data(), bufferDesc.size);
	m_axesVertexCount = static_cast<int>(axesVertexData.size());

	return m_axesVertexBuffer != nullptr;
}


bool Application::initPrincipalPlanesWireframeGeometry(float spacing, float len) {
	
	std::vector<VertexAttributes> axesVertexData = {
    	// X axis (red)
		{ {-len, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
		{ { len, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },

		// Y axis (green)
		{ {0.0f, -len, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },
		{ {0.0f,  len, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },

		// Z axis (blue)
		{ {0.0f, 0.0f, -len}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {0.0f, 0.0f,  len}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
	};


	for (float i = -len; i <= len; i += spacing) {

		axesVertexData.insert(axesVertexData.end(), {
			
			// --- XY Plane ---

			// X axis wireframe
			{ {-len, i, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			{ { len, i, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },

			// Y axis wireframe
			{ {i, -len, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			{ {i, len, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			
			// --- XZ Plane ---

			// X axis wireframe
			{ {-len, 0.0f, i}, {0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f} },
			{ { len, 0.0f, i}, {0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f} },

			// Z axis wireframe
			{ {i, 0.0f, -len}, {0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f} },
			{ {i, 0.0f, len}, {0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f} },
			
			// --- YZ Plane ---
			
			// Y axis wireframe
			{ {0.0f, -len, i}, {0.0f, 0.0f, 0.0f}, {0.25f, 0.25f, 0.25f}, {0.0f, 0.0f} },
			{ {0.0f, len, i}, {0.0f, 0.0f, 0.0f}, {0.25f, 0.25f, 0.25f}, {0.0f, 0.0f} },

			// Z axis wireframe
			{ {0.0f, i, -len}, {0.0f, 0.0f, 0.0f}, {0.25f, 0.25f, 0.25f}, {0.0f, 0.0f} },
			{ {0.0f, i, len}, {0.0f, 0.0f, 0.0f}, {0.25f, 0.25f, 0.25f}, {0.0f, 0.0f} },
		});

	}

	WGPUBufferDescriptor bufferDesc = {};
	bufferDesc.size = axesVertexData.size() * sizeof(VertexAttributes);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
	bufferDesc.mappedAtCreation = false;
	m_axesVertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	wgpuQueueWriteBuffer(m_queue, m_axesVertexBuffer, 0, axesVertexData.data(), bufferDesc.size);
	m_axesVertexCount = static_cast<int>(axesVertexData.size());

	return m_axesVertexBuffer != nullptr;
}


bool Application::initWireframeGeometry(float spacing, float len) {
	
	std::vector<VertexAttributes> axesVertexData = {
    	// X axis (red)
		{ {-len, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
		{ { len, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },

		// Y axis (green)
		{ {0.0f, -len, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },
		{ {0.0f,  len, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },

		// Z axis (blue)
		{ {0.0f, 0.0f, -len}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {0.0f, 0.0f,  len}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
	};

	for (float j = -len; j <= len; j += spacing) {
		for (float i = -len; i <= len; i += spacing) {

			axesVertexData.insert(axesVertexData.end(), {
				
				// --- XY Plane ---

				// X axis wireframe
				{ {-len, i, j}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
				{ { len, i, j}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },

				// Y axis wireframe
				{ {i, -len, j}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
				{ {i, len, j}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
				
				// --- XZ Plane ---

				// X axis wireframe
				{ {-len, j, i}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
				{ { len, j, i}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },

				// Z axis wireframe
				{ {i, j, -len}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
				{ {i, j, len}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
				
				// --- YZ Plane ---
				
				// Y axis wireframe
				{ {j, -len, i}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
				{ {j, len, i}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },

				// Z axis wireframe
				{ {j, i, -len}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
				{ {j, i, len}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
			});
		}
	}

	WGPUBufferDescriptor bufferDesc = {};
	bufferDesc.size = axesVertexData.size() * sizeof(VertexAttributes);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
	bufferDesc.mappedAtCreation = false;
	m_axesVertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	wgpuQueueWriteBuffer(m_queue, m_axesVertexBuffer, 0, axesVertexData.data(), bufferDesc.size);
	m_axesVertexCount = static_cast<int>(axesVertexData.size());

	return m_axesVertexBuffer != nullptr;
}


bool Application::initArrowGeometry(float length, float headLength, float headAngle) {
    std::vector<VertexAttributes> arrowVertexData;

    // Define the shaft (line)
    arrowVertexData.insert(arrowVertexData.end(), {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{0.0f, 0.0f, length}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}
    });

    // Define the arrowhead (4 lines forming a cross)
    glm::vec3 tip(0.0f, 0.0f, length);
    glm::vec3 leftWing = tip + glm::vec3(headLength * cos(headAngle), headLength * sin(headAngle), -headLength);
    glm::vec3 rightWing = tip + glm::vec3(-headLength * cos(headAngle), headLength * sin(headAngle), -headLength);
    glm::vec3 topWing = tip + glm::vec3(headLength * sin(headAngle), headLength * cos(headAngle), -headLength);
    glm::vec3 bottomWing = tip + glm::vec3(-headLength * sin(headAngle), headLength * cos(headAngle), -headLength);

    arrowVertexData.insert(arrowVertexData.end(), {
        {tip, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {leftWing, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        
        {tip, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {rightWing, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        
        {tip, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {topWing, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        
        {tip, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {bottomWing, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}
    });

    // Create the buffer and upload the data
    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.size = arrowVertexData.size() * sizeof(VertexAttributes);
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    bufferDesc.mappedAtCreation = false;
    m_arrowVertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
    wgpuQueueWriteBuffer(m_queue, m_arrowVertexBuffer, 0, arrowVertexData.data(), bufferDesc.size);
    m_arrowVertexCount = static_cast<int>(arrowVertexData.size());

    return m_arrowVertexBuffer != nullptr;
}

void Application::terminateGeometry() {
	// m_vertexBuffer.destroy();
	// m_vertexBuffer.release();
	wgpuBufferDestroy(m_vertexBuffer);
	wgpuBufferRelease(m_vertexBuffer);
	m_vertexCount = 0;
}
void Application::terminateAxesGeometry() {
	// m_vertexBuffer.destroy();
	// m_vertexBuffer.release();
	wgpuBufferDestroy(m_axesVertexBuffer);
	wgpuBufferRelease(m_axesVertexBuffer);
	m_axesVertexCount = 0;
}


bool Application::initUniforms(float fovy, float aspectRatio, float nearPlane, float farPlane) {
	// Create uniform buffer
	WGPUBufferDescriptor bufferDesc = {};
	bufferDesc.size = sizeof(MyUniforms);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
	bufferDesc.mappedAtCreation = false;

	std::cout << "Creating Uniform Buffer..." << std::endl;
	std::cout << "Creating Uniform Buffer using m_device: " << m_device << std::endl;

	m_uniformBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);

	std::cout << "Uniform Buffer created" << std::endl;

	// Upload the initial value of the uniforms
	m_uniforms.modelMatrix = mat4x4(1.0); // 4x4 Identity
	m_uniforms.viewMatrix = glm::lookAt(vec3(-2.0f, -3.0f, 2.0f), vec3(0.0f), vec3(0, 0, 1));

	// m_fovy = glm::radians(180.0f);
    // m_aspectRatio = 640.0f / 480.0f;
    // m_nearPlane = 0.01f;
    // m_farPlane = 100.0f; 

	m_fovy = fovy;
	m_aspectRatio = aspectRatio;
	m_nearPlane = nearPlane;
	m_farPlane = farPlane;

    m_uniforms.projectionMatrix = glm::perspective(m_fovy, m_aspectRatio, m_nearPlane, m_farPlane);
	m_uniforms.time = 1.0f;
	m_uniforms.color = { 0.0f, 1.0f, 0.4f, 1.0f };
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0, &m_uniforms, sizeof(MyUniforms));

	std::cout << "Uniform Buffer written" << std::endl;

	updateProjectionMatrix();
	updateViewMatrix();

	std::cout << "Uniforms successfully initialised" << std::endl;

	return m_uniformBuffer != nullptr;
}

void Application::terminateUniforms() {
	wgpuBufferDestroy(m_uniformBuffer);
	wgpuBufferRelease(m_uniformBuffer);
}


bool Application::initLightingUniforms() {
	// Create uniform buffer
	WGPUBufferDescriptor bufferDesc = {};
	bufferDesc.size = sizeof(LightingUniforms);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
	bufferDesc.mappedAtCreation = false;
	//m_lightingUniformBuffer = m_device.createBuffer(bufferDesc);
	m_lightingUniformBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);

	// Initial values
	m_lightingUniforms.directions[0] = { 0.5f, -0.9f, 0.1f, 0.0f };
	m_lightingUniforms.directions[1] = { 0.2f, 0.4f, 0.3f, 0.0f };
	m_lightingUniforms.colors[0] = { 1.0f, 0.9f, 0.6f, 1.0f };
	m_lightingUniforms.colors[1] = { 0.6f, 0.9f, 1.0f, 1.0f };

	updateLightingUniforms();

	return m_lightingUniformBuffer != nullptr;
}

void Application::terminateLightingUniforms() {
	wgpuBufferDestroy(m_lightingUniformBuffer);
	wgpuBufferRelease(m_lightingUniformBuffer);
	// m_lightingUniformBuffer.destroy();
	// m_lightingUniformBuffer.release();
}

void Application::updateLightingUniforms() {
	if (m_lightingUniformsChanged) {
		wgpuQueueWriteBuffer(m_queue, m_lightingUniformBuffer, 0, &m_lightingUniforms, sizeof(LightingUniforms));
		m_lightingUniformsChanged = false;
	}
}


bool Application::initBindGroupLayout() {
	std::vector<WGPUBindGroupLayoutEntry> bindingLayoutEntries(4, WGPUBindGroupLayoutEntry{});

	// The uniform buffer binding
	WGPUBindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
	bindingLayout.binding = 0;
	bindingLayout.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
	bindingLayout.buffer.type = WGPUBufferBindingType_Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

	// The texture binding
	WGPUBindGroupLayoutEntry& textureBindingLayout = bindingLayoutEntries[1];
	textureBindingLayout.binding = 1;
	textureBindingLayout.visibility = WGPUShaderStage_Fragment;
	textureBindingLayout.texture.sampleType = WGPUTextureSampleType_Float;
	textureBindingLayout.texture.viewDimension = WGPUTextureViewDimension_2D;

	// The texture sampler binding
	WGPUBindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[2];
	samplerBindingLayout.binding = 2;
	samplerBindingLayout.visibility = WGPUShaderStage_Fragment;
	samplerBindingLayout.sampler.type = WGPUSamplerBindingType_Filtering;

	// The lighting uniform buffer binding
	WGPUBindGroupLayoutEntry& lightingUniformLayout = bindingLayoutEntries[3];
	lightingUniformLayout.binding = 3;
	lightingUniformLayout.visibility = WGPUShaderStage_Fragment; // only Fragment is needed
	lightingUniformLayout.buffer.type = WGPUBufferBindingType_Uniform;
	lightingUniformLayout.buffer.minBindingSize = sizeof(LightingUniforms);

	// Create a bind group layout
	WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
	bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
	m_bindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bindGroupLayoutDesc);
	// m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

	return m_bindGroupLayout != nullptr;
}

void Application::terminateBindGroupLayout() {
	wgpuBindGroupLayoutRelease(m_bindGroupLayout);
}


bool Application::initBindGroup() {
	// Create a binding
	std::vector<WGPUBindGroupEntry> bindings(4, WGPUBindGroupEntry{});

	bindings[0].binding = 0;
	bindings[0].buffer = m_uniformBuffer;
	bindings[0].offset = 0;
	bindings[0].size = sizeof(MyUniforms);

	bindings[1].binding = 1;
	bindings[1].textureView = m_textureView;

	bindings[2].binding = 2;
	bindings[2].sampler = m_sampler;

	bindings[3].binding = 3;
	bindings[3].buffer = m_lightingUniformBuffer;
	bindings[3].offset = 0;
	bindings[3].size = sizeof(LightingUniforms);

	WGPUBindGroupDescriptor bindGroupDesc = {};
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)bindings.size();
	bindGroupDesc.entries = bindings.data();
	m_bindGroup = wgpuDeviceCreateBindGroup(m_device, &bindGroupDesc);

	return m_bindGroup != nullptr;
}

void Application::terminateBindGroup() {
	wgpuBindGroupRelease(m_bindGroup);
}

void Application::updateProjectionMatrix() {
	// Update projection matrix
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	float ratio = width / (float)height;
	m_aspectRatio = ratio;
	m_uniforms.projectionMatrix = glm::perspective(m_fovy, m_aspectRatio, m_nearPlane, m_farPlane);
	wgpuQueueWriteBuffer(
		m_queue,
		m_uniformBuffer,
		offsetof(MyUniforms, projectionMatrix),
		&m_uniforms.projectionMatrix,
		sizeof(MyUniforms::projectionMatrix)
	);
}

void Application::updateViewMatrix() {
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	m_uniforms.viewMatrix = glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1));
	
	wgpuQueueWriteBuffer(
		m_queue,
		m_uniformBuffer,
		offsetof(MyUniforms, viewMatrix),
		&m_uniforms.viewMatrix,
		sizeof(MyUniforms::viewMatrix)
	);

	m_uniforms.cameraWorldPosition = position;

	wgpuQueueWriteBuffer(
		m_queue,
		m_uniformBuffer,
		offsetof(MyUniforms, cameraWorldPosition),
		&m_uniforms.cameraWorldPosition,
		sizeof(MyUniforms::cameraWorldPosition)
	);

}

void Application::updateDragInertia() {
	constexpr float eps = 1e-4f;
	// Apply inertia only when the user released the click.
	if (!m_drag.active) {
		// Avoid updating the matrix when the velocity is no longer noticeable
		if (std::abs(m_drag.velocity.x) < eps && std::abs(m_drag.velocity.y) < eps) {
			return;
		}
		m_cameraState.angles += m_drag.velocity;
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
		// Dampen the velocity so that it decreases exponentially and stops
		// after a few frames.
		m_drag.velocity *= m_drag.intertia;
		updateViewMatrix();
	}
}

// ─── Axes Rebuild ───────────────────────────────────────────────────────────

void Application::rebuildAxesBuffer() {
	// Defer old buffer destruction
	if (m_axesVertexBuffer) {
		deferBufferRelease(m_axesVertexBuffer);
		m_axesVertexBuffer = nullptr;
		m_axesVertexCount = 0;
	}

	std::vector<VertexAttributes> axesVertexData;
	float len = m_gridExtent;

	// Principal axes
	axesVertexData.push_back({{-len, 0, 0}, {0,0,0}, {m_axisColors[0][0], m_axisColors[0][1], m_axisColors[0][2]}, {0,0}});
	axesVertexData.push_back({{ len, 0, 0}, {0,0,0}, {m_axisColors[0][0], m_axisColors[0][1], m_axisColors[0][2]}, {0,0}});
	axesVertexData.push_back({{0, -len, 0}, {0,0,0}, {m_axisColors[1][0], m_axisColors[1][1], m_axisColors[1][2]}, {0,0}});
	axesVertexData.push_back({{0,  len, 0}, {0,0,0}, {m_axisColors[1][0], m_axisColors[1][1], m_axisColors[1][2]}, {0,0}});
	axesVertexData.push_back({{0, 0, -len}, {0,0,0}, {m_axisColors[2][0], m_axisColors[2][1], m_axisColors[2][2]}, {0,0}});
	axesVertexData.push_back({{0, 0,  len}, {0,0,0}, {m_axisColors[2][0], m_axisColors[2][1], m_axisColors[2][2]}, {0,0}});

	if (m_showGrid) {
		vec3 gc = {m_gridColor[0], m_gridColor[1], m_gridColor[2]};
		for (float i = -len; i <= len; i += m_gridSpacing) {
			if (m_showXYGrid) {
				axesVertexData.push_back({{-len, i, 0}, {0,0,0}, gc, {0,0}});
				axesVertexData.push_back({{ len, i, 0}, {0,0,0}, gc, {0,0}});
				axesVertexData.push_back({{i, -len, 0}, {0,0,0}, gc, {0,0}});
				axesVertexData.push_back({{i,  len, 0}, {0,0,0}, gc, {0,0}});
			}
			if (m_showXZGrid) {
				axesVertexData.push_back({{-len, 0, i}, {0,0,0}, gc, {0,0}});
				axesVertexData.push_back({{ len, 0, i}, {0,0,0}, gc, {0,0}});
				axesVertexData.push_back({{i, 0, -len}, {0,0,0}, gc, {0,0}});
				axesVertexData.push_back({{i, 0,  len}, {0,0,0}, gc, {0,0}});
			}
			if (m_showYZGrid) {
				axesVertexData.push_back({{0, -len, i}, {0,0,0}, gc, {0,0}});
				axesVertexData.push_back({{0,  len, i}, {0,0,0}, gc, {0,0}});
				axesVertexData.push_back({{0, i, -len}, {0,0,0}, gc, {0,0}});
				axesVertexData.push_back({{0, i,  len}, {0,0,0}, gc, {0,0}});
			}
		}
	}

	if (!axesVertexData.empty()) {
		WGPUBufferDescriptor bufferDesc = {};
		bufferDesc.size = axesVertexData.size() * sizeof(VertexAttributes);
		bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
		bufferDesc.mappedAtCreation = false;
		m_axesVertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
		wgpuQueueWriteBuffer(m_queue, m_axesVertexBuffer, 0, axesVertexData.data(), bufferDesc.size);
		m_axesVertexCount = static_cast<int>(axesVertexData.size());
	}

	m_axesDirty = false;
}

// ─── Expression Compilation ─────────────────────────────────────────────────

void Application::compileFunctionDef(FunctionDefinition& fd) {
	// Build variable name list based on inputDim
	std::vector<std::string> varNames;
	for (int i = 0; i < fd.inputDim; ++i) {
		if (!fd.paramNames[i].empty())
			varNames.push_back(fd.paramNames[i]);
	}

	// Add "pi" and "e" as extra variables bound to constants
	// (tinyexpr has built-in pi and e, so we don't need to add them)

	fd.isValid = true;
	fd.errorMsg.clear();

	for (int i = 0; i < fd.outputDim; ++i) {
		std::string err;
		if (!fd.parsers[i].compile(fd.exprStrings[i], varNames, err)) {
			fd.isValid = false;
			fd.errorMsg = "f" + std::to_string(i + 1) + ": " + err;
			return;
		}
	}

	// Auto-enable vector field overlay for R³→R³ functions
	if (fd.inputDim == 3 && fd.outputDim == 3) {
		fd.showVectorField = true;
	}
}

// ─── Graph Objects ──────────────────────────────────────────────────────────

bool Application::initGraphObjects() {
	// Create a default helix function
	FunctionDefinition helix;
	helix.name = "r";
	helix.inputDim = 1;
	helix.outputDim = 3;
	helix.paramNames[0] = "t";
	helix.exprStrings[0] = "cos(t)";
	helix.exprStrings[1] = "sin(t)";
	helix.exprStrings[2] = "t/(2*pi)";
	helix.rangeMin[0] = -6.283f;  // -2π to 2π for helix
	helix.rangeMax[0] = 6.283f;
	helix.resolution[0] = 200;
	helix.tubeRadius = 0.03f;
	helix.color[0] = 1.0f; helix.color[1] = 1.0f; helix.color[2] = 0.0f;
	compileFunctionDef(helix);
	m_functions.push_back(std::move(helix));

	updateGraphObjects();
	return true;
}

void Application::terminateGraphObjects() {
	if (m_surfaceVertexBuffer) {
		wgpuBufferDestroy(m_surfaceVertexBuffer);
		wgpuBufferRelease(m_surfaceVertexBuffer);
		m_surfaceVertexBuffer = nullptr;
		m_surfaceVertexCount = 0;
	}
	if (m_curveVertexBuffer) {
		wgpuBufferDestroy(m_curveVertexBuffer);
		wgpuBufferRelease(m_curveVertexBuffer);
		m_curveVertexBuffer = nullptr;
		m_curveVertexCount = 0;
	}
	// Flush all pending buffer releases
	for (auto& p : m_pendingBufferReleases) {
		wgpuBufferDestroy(p.first);
		wgpuBufferRelease(p.first);
	}
	m_pendingBufferReleases.clear();
}

void Application::updateGraphObjects() {
	if (m_axesDirty) {
		rebuildAxesBuffer();
	}

	if (!m_graphObjectsDirty) return;

	// Throttle updates to prevent buffer churn (wait at least 2 frames)
	m_framesSinceLastUpdate++;
	if (m_framesSinceLastUpdate < 2) return;
	m_framesSinceLastUpdate = 0;

	m_graphObjectsDirty = false;

	// Defer old buffer destruction (GPU may still be using them)
	if (m_surfaceVertexBuffer) {
		deferBufferRelease(m_surfaceVertexBuffer);
		m_surfaceVertexBuffer = nullptr;
		m_surfaceVertexCount = 0;
	}
	if (m_curveVertexBuffer) {
		deferBufferRelease(m_curveVertexBuffer);
		m_curveVertexBuffer = nullptr;
		m_curveVertexCount = 0;
	}

	// TriangleList geometry (lit, "surface" pipeline)
	std::vector<VertexAttributes> surfaceVerts;
	// LineList geometry (unlit, "axes" pipeline) — for wireframe overlays
	std::vector<VertexAttributes> lineVerts;

	for (auto& fd : m_functions) {
		if (!fd.show || !fd.isValid) continue;

		vec3 col(fd.color[0], fd.color[1], fd.color[2]);
		int n = fd.inputDim;
		int m = fd.outputDim;

		if (n == 1) {
			// Curve: build lambda t -> vec3
			auto curveFunc = [&fd, m](float t) -> glm::vec3 {
				double vals[1] = { (double)t };
				if (m == 1) {
					float fx = (float)fd.parsers[0].evaluate(vals);
					return glm::vec3(t, fx, 0.0f);
				} else if (m == 2) {
					float fx = (float)fd.parsers[0].evaluate(vals);
					float fy = (float)fd.parsers[1].evaluate(vals);
					// Map to selected plane: 0=xy, 1=xz, 2=yz
					if (fd.curvePlane == 0) return glm::vec3(fx, fy, 0.0f);      // xy plane
					else if (fd.curvePlane == 1) return glm::vec3(fx, 0.0f, fy); // xz plane
					else return glm::vec3(0.0f, fx, fy);                          // yz plane
				} else { // m == 3
					float fx = (float)fd.parsers[0].evaluate(vals);
					float fy = (float)fd.parsers[1].evaluate(vals);
					float fz = (float)fd.parsers[2].evaluate(vals);
					return glm::vec3(fx, fy, fz);
				}
			};

			// Use thinner tube for 2D curves, and support wireframe mode
			float tubeRad = (m == 2) ? 0.01f : fd.tubeRadius;
			if (!fd.wireframe) {
				auto verts = GraphObjects::generateParametricCurveTube(
					curveFunc, fd.rangeMin[0], fd.rangeMax[0],
					fd.resolution[0], tubeRad, 8, col);
				surfaceVerts.insert(surfaceVerts.end(), verts.begin(), verts.end());
			} else {
				// Wireframe: use line rendering with purple color
				vec3 wireframeColor(0.7f, 0.4f, 0.8f);
				auto curveLineVerts = GraphObjects::generateParametricCurve(
					curveFunc, fd.rangeMin[0], fd.rangeMax[0],
					fd.resolution[0], wireframeColor);
				lineVerts.insert(lineVerts.end(), curveLineVerts.begin(), curveLineVerts.end());
			}

			// Tangent vectors overlay
			if (fd.showTangentVectors) {
				auto tangentVerts = GraphObjects::generateTangentVectors(
					curveFunc, fd.rangeMin[0], fd.rangeMax[0],
					fd.overlayVectorCount, fd.overlayVectorScale, vec3(1, 0, 0));
				surfaceVerts.insert(surfaceVerts.end(), tangentVerts.begin(), tangentVerts.end());
			}

			// Normal vectors overlay for curves
			if (fd.showNormalVectors) {
				auto normalVerts = GraphObjects::generateCurveNormals(
					curveFunc, fd.rangeMin[0], fd.rangeMax[0],
					fd.overlayVectorCount, fd.overlayVectorScale, vec3(0, 1, 0), fd.flipNormalVectors);
				surfaceVerts.insert(surfaceVerts.end(), normalVerts.begin(), normalVerts.end());
			}

			// Frenet frame overlay
			if (fd.showFrenetFrame) {
				auto frenetVerts = GraphObjects::generateFrenetFrame(
					curveFunc, fd.rangeMin[0], fd.rangeMax[0],
					fd.frenetT, fd.overlayVectorScale);
				surfaceVerts.insert(surfaceVerts.end(), frenetVerts.begin(), frenetVerts.end());
			}

		} else if (n == 2) {
			// Surface: build lambda (u,v) -> vec3
			auto surfFunc = [&fd, m](float u, float v) -> glm::vec3 {
				double vals[2] = { (double)u, (double)v };
				if (m == 1) {
					float fval = (float)fd.parsers[0].evaluate(vals);
					return glm::vec3(u, v, fval);
				} else if (m == 2) {
					float fx = (float)fd.parsers[0].evaluate(vals);
					float fy = (float)fd.parsers[1].evaluate(vals);
					return glm::vec3(fx, fy, 0.0f);
				} else { // m == 3
					float fx = (float)fd.parsers[0].evaluate(vals);
					float fy = (float)fd.parsers[1].evaluate(vals);
					float fz = (float)fd.parsers[2].evaluate(vals);
					return glm::vec3(fx, fy, fz);
				}
			};

			if (fd.wireframe) {
				// Wireframe: LineList via axes pipeline with purple color
				vec3 wireframeColor(0.7f, 0.4f, 0.8f);
				auto wfVerts = GraphObjects::generateParametricSurfaceWireframe(
					surfFunc,
					fd.rangeMin[0], fd.rangeMax[0],
					fd.rangeMin[1], fd.rangeMax[1],
					fd.resolution[0], fd.resolution[1], wireframeColor);
				lineVerts.insert(lineVerts.end(), wfVerts.begin(), wfVerts.end());
			} else {
				// Filled surface
				auto verts = GraphObjects::generateParametricSurface(
					surfFunc,
					fd.rangeMin[0], fd.rangeMax[0],
					fd.rangeMin[1], fd.rangeMax[1],
					fd.resolution[0], fd.resolution[1], true);
				surfaceVerts.insert(surfaceVerts.end(), verts.begin(), verts.end());
			}

			// Normal vectors overlay
			if (fd.showNormalVectors) {
				int nCount = std::max(fd.overlayVectorCount, 2);
				auto normalVerts = GraphObjects::generateSurfaceNormals(
					surfFunc,
					fd.rangeMin[0], fd.rangeMax[0],
					fd.rangeMin[1], fd.rangeMax[1],
					nCount, nCount,
					fd.overlayVectorScale, vec3(0.2f, 0.4f, 1.0f), fd.flipNormalVectors);
				surfaceVerts.insert(surfaceVerts.end(), normalVerts.begin(), normalVerts.end());
			}

			// Tangent vectors overlay for surfaces
			if (fd.showTangentVectors) {
				int tCount = std::max(fd.overlayVectorCount, 2);
				auto tangentVerts = GraphObjects::generateSurfaceTangents(
					surfFunc,
					fd.rangeMin[0], fd.rangeMax[0],
					fd.rangeMin[1], fd.rangeMax[1],
					tCount, tCount,
					fd.overlayVectorScale, vec3(1.0f, 0.2f, 0.2f), fd.surfaceTangentMode);
				surfaceVerts.insert(surfaceVerts.end(), tangentVerts.begin(), tangentVerts.end());
			}

			// Gradient field overlay (only for R^2->R^1)
			if (fd.showGradientField && m == 1) {
				auto scalarFunc2D = [&fd](float u, float v) -> float {
					double vals[2] = { (double)u, (double)v };
					return (float)fd.parsers[0].evaluate(vals);
				};
				int gCount = std::max(fd.overlayVectorCount, 2);
				auto gradVerts = GraphObjects::generateGradientField2D(
					scalarFunc2D,
					fd.rangeMin[0], fd.rangeMax[0],
					fd.rangeMin[1], fd.rangeMax[1],
					gCount, gCount,
					fd.overlayVectorScale);
				surfaceVerts.insert(surfaceVerts.end(), gradVerts.begin(), gradVerts.end());
			}

		} else if (n == 3) {
			vec3 rMin(fd.rangeMin[0], fd.rangeMin[1], fd.rangeMin[2]);
			vec3 rMax(fd.rangeMax[0], fd.rangeMax[1], fd.rangeMax[2]);
			glm::ivec3 res(fd.overlayVectorCount);  // Use overlayVectorCount for both arrows and streamlines

			if (m == 1) {
				// Scalar field: colored cubes
				auto scalarFunc = [&fd](glm::vec3 p) -> float {
					double vals[3] = { (double)p.x, (double)p.y, (double)p.z };
					return (float)fd.parsers[0].evaluate(vals);
				};

				auto verts = GraphObjects::generateScalarField(
					scalarFunc, rMin, rMax, res, 0.1f);
				surfaceVerts.insert(surfaceVerts.end(), verts.begin(), verts.end());

				// Gradient field overlay for R^3->R^1
				if (fd.showGradientField) {
					auto gradVerts = GraphObjects::generateGradientField3D(
						scalarFunc, rMin, rMax, res, fd.overlayVectorScale);
					surfaceVerts.insert(surfaceVerts.end(), gradVerts.begin(), gradVerts.end());
				}

			} else {
				// Vector field (m==2 or m==3)
				auto fieldFunc = [&fd, m](glm::vec3 p) -> glm::vec3 {
					double vals[3] = { (double)p.x, (double)p.y, (double)p.z };
					float fx = (float)fd.parsers[0].evaluate(vals);
					float fy = (float)fd.parsers[1].evaluate(vals);
					float fz = (m >= 3) ? (float)fd.parsers[2].evaluate(vals) : 0.0f;
					return glm::vec3(fx, fy, fz);
				};

				// Show vector field arrows
				if (fd.showVectorField) {
					auto verts = GraphObjects::generateVectorField(
						fieldFunc, rMin, rMax, res, fd.arrowScale);
					surfaceVerts.insert(surfaceVerts.end(), verts.begin(), verts.end());
				}

				// Show streamlines
				if (fd.showStreamlines) {
					auto streamVerts = GraphObjects::generateStreamlines(
						fieldFunc, rMin, rMax, res, fd.overlayVectorCount, fd.overlayVectorScale);
					lineVerts.insert(lineVerts.end(), streamVerts.begin(), streamVerts.end());
				}
			}
		}
	}

	// Upload surface buffer (TriangleList)
	if (!surfaceVerts.empty()) {
		WGPUBufferDescriptor bufferDesc = {};
		bufferDesc.size = surfaceVerts.size() * sizeof(VertexAttributes);
		bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
		bufferDesc.mappedAtCreation = false;
		m_surfaceVertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
		wgpuQueueWriteBuffer(m_queue, m_surfaceVertexBuffer, 0, surfaceVerts.data(), bufferDesc.size);
		m_surfaceVertexCount = static_cast<int>(surfaceVerts.size());
	}

	// Upload wireframe line buffer (LineList)
	if (!lineVerts.empty()) {
		WGPUBufferDescriptor bufferDesc = {};
		bufferDesc.size = lineVerts.size() * sizeof(VertexAttributes);
		bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
		bufferDesc.mappedAtCreation = false;
		m_curveVertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
		wgpuQueueWriteBuffer(m_queue, m_curveVertexBuffer, 0, lineVerts.data(), bufferDesc.size);
		m_curveVertexCount = static_cast<int>(lineVerts.size());
	}
}

bool Application::initGui() {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplWGPU_Init(m_device, 3, m_swapChainFormat, m_depthTextureFormat);
	return true;
}

void Application::terminateGui() {
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}

void Application::updateGui(WGPURenderPassEncoder renderPass) {
	// Start the Dear ImGui frame
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Build our UI
	{
		bool dirty = false;
		bool lightingChanged = false;

		ImGui::Begin("Visualization");
		ImGui::PushItemWidth(150);

		// ── Functions ──
		int removeIdx = -1;
		for (int fi = 0; fi < (int)m_functions.size(); ++fi) {
			auto& fd = m_functions[fi];
			ImGui::PushID(fi);

			// Build header label: "r(t)" or "S(u,v)" etc.
			std::string params;
			for (int i = 0; i < fd.inputDim; ++i) {
				if (i > 0) params += ",";
				params += fd.paramNames[i];
			}
			std::string headerLabel = fd.name + "(" + params + ")###func" + std::to_string(fi);

			if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Text("Show"); ImGui::SameLine(); dirty |= ImGui::Checkbox("##show", &fd.show);
				ImGui::SameLine();

				// Input dim selector
				int prevInputDim = fd.inputDim;
				ImGui::Text("In"); ImGui::SameLine();
				ImGui::SetNextItemWidth(80);
				const char* inputDimLabels[] = {"1", "2", "3"};
				int inputDimIdx = fd.inputDim - 1;
				if (ImGui::Combo("##indim", &inputDimIdx, inputDimLabels, 3)) {
					fd.inputDim = inputDimIdx + 1;
					dirty = true;
				}
				ImGui::SameLine();

				// Output dim selector
				ImGui::Text("Out"); ImGui::SameLine();
				ImGui::SetNextItemWidth(80);
				int outputDimIdx = fd.outputDim - 1;
				if (ImGui::Combo("##outdim", &outputDimIdx, inputDimLabels, 3)) {
					fd.outputDim = outputDimIdx + 1;
					dirty = true;
				}

				// Reset param names when inputDim changes
				if (fd.inputDim != prevInputDim) {
					if (fd.inputDim == 1) { fd.paramNames[0] = "t"; fd.paramNames[1] = ""; fd.paramNames[2] = ""; }
					else if (fd.inputDim == 2) { fd.paramNames[0] = "x"; fd.paramNames[1] = "y"; fd.paramNames[2] = ""; }  // Use x,y for surfaces
					else { fd.paramNames[0] = "x"; fd.paramNames[1] = "y"; fd.paramNames[2] = "z"; }
					// Clear extra expressions
					for (int i = 0; i < 3; ++i) fd.exprStrings[i] = "";
				}

				// Name + param names
				ImGui::Text("Function"); ImGui::SameLine();
				ImGui::SetNextItemWidth(60);
				dirty |= ImGui::InputText("##name", &fd.name);
				ImGui::SameLine();
				ImGui::Text("Variable(s)"); ImGui::SameLine();
				for (int i = 0; i < fd.inputDim; ++i) {
					ImGui::SameLine();
					ImGui::SetNextItemWidth(40);
					std::string paramLabel = "##param" + std::to_string(i);
					dirty |= ImGui::InputText(paramLabel.c_str(), &fd.paramNames[i]);
				}

				// Expression text fields
				for (int i = 0; i < fd.outputDim; ++i) {
					std::string labelText = (fd.outputDim == 1) ? fd.name : fd.name + std::to_string(i + 1);
					ImGui::Text("%s", labelText.c_str()); ImGui::SameLine();
					std::string label = "##expr" + std::to_string(i);
					dirty |= ImGui::InputText(label.c_str(), &fd.exprStrings[i]);
				}

				// Error message
				if (!fd.isValid && !fd.errorMsg.empty()) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.2f, 0.2f, 1));
					ImGui::TextWrapped("%s", fd.errorMsg.c_str());
					ImGui::PopStyleColor();
				}

				// Presets
				{
					int n = fd.inputDim, m = fd.outputDim;
					std::vector<std::pair<std::string, std::vector<std::string>>> presets;
					std::vector<std::array<float, 6>> presetRanges; // min0,max0,min1,max1,min2,max2

					if (n == 1 && m == 3) {
						presets = {
							{"Helix", {"cos(t)", "sin(t)", "t/(2*pi)"}},
							{"Trefoil", {"sin(t)+2*sin(2*t)", "cos(t)-2*cos(2*t)", "-sin(3*t)"}},
							{"Lissajous", {"2*sin(2*t)", "2*sin(3*t)", "2*cos(5*t)"}},
							{"Straight Line", {"t", "t", "t"}},
							{"Viviani", {"cos(t)^2", "cos(t)*sin(t)", "sin(t)"}},
							{"Toroidal Spiral", {"(2+cos(3*t))*cos(t)", "(2+cos(3*t))*sin(t)", "sin(3*t)"}},
							{"Conical Helix", {"t*cos(3*t)", "t*sin(3*t)", "t"}},
						};
						presetRanges = {
							{0, 6.283f, 0, 0, 0, 0},      // Helix: 0 to 2π
							{0, 6.283f, 0, 0, 0, 0},      // Trefoil: 0 to 2π
							{0, 6.283f, 0, 0, 0, 0},      // Lissajous: 0 to 2π
							{-5, 5, 0, 0, 0, 0},          // Straight Line: -5 to 5
							{0, 6.283f, 0, 0, 0, 0},      // Viviani: 0 to 2π
							{0, 6.283f, 0, 0, 0, 0},      // Toroidal Spiral: 0 to 2π
							{0, 6.283f, 0, 0, 0, 0},      // Conical Helix: 0 to 2π
						};
					} else if (n == 1 && m == 1) {
						presets = {
							{"sin(t)", {"sin(t)"}},
							{"t^2", {"t^2"}},
							{"exp(-t^2)", {"exp(-t^2)"}},
						};
						presetRanges = {
							{-5, 5, 0, 0, 0, 0},
							{-5, 5, 0, 0, 0, 0},
							{-5, 5, 0, 0, 0, 0},
						};
					} else if (n == 1 && m == 2) {
						presets = {
							{"Circle", {"cos(t)", "sin(t)"}},
							{"Lemniscate", {"cos(t)/(1+sin(t)^2)", "sin(t)*cos(t)/(1+sin(t)^2)"}},
							{"Spiral", {"t*cos(t)", "t*sin(t)"}},
							{"Rose (4-petal)", {"cos(2*t)*cos(t)", "cos(2*t)*sin(t)"}},
							{"Cardioid", {"(1-cos(t))*cos(t)", "(1-cos(t))*sin(t)"}},
							{"Butterfly", {"sin(t)*(exp(cos(t))-2*cos(4*t)-sin(t/12)^5)", "cos(t)*(exp(cos(t))-2*cos(4*t)-sin(t/12)^5)"}},
							{"Epitrochoid", {"(5*cos(t)-2*cos(5*t/2))", "(5*sin(t)-2*sin(5*t/2))"}},
						};
						presetRanges = {
							{0, 6.283f, 0, 0, 0, 0},      // Circle: 0 to 2π
							{0, 6.283f, 0, 0, 0, 0},      // Lemniscate: 0 to 2π
							{0, 12.566f, 0, 0, 0, 0},     // Spiral: 0 to 4π
							{0, 6.283f, 0, 0, 0, 0},      // Rose: 0 to 2π
							{0, 6.283f, 0, 0, 0, 0},      // Cardioid: 0 to 2π
							{0, 12.566f, 0, 0, 0, 0},     // Butterfly: 0 to 4π
							{0, 12.566f, 0, 0, 0, 0},     // Epitrochoid: 0 to 4π
						};
					} else if (n == 2 && m == 3) {
						presets = {
							{"Sphere", {"2*cos(u)*sin(v)", "2*sin(u)*sin(v)", "2*cos(v)"}},
							{"Torus", {"(2+0.7*cos(v))*cos(u)", "(2+0.7*cos(v))*sin(u)", "0.7*sin(v)"}},
							{"Mobius", {"(1+v/2*cos(u/2))*cos(u)", "(1+v/2*cos(u/2))*sin(u)", "v/2*sin(u/2)"}},
							{"Hyperboloid", {"cosh(v)*cos(u)", "cosh(v)*sin(u)", "sinh(v)"}},
							{"Klein Bottle", {"(2+cos(v/2)*sin(u)-sin(v/2)*sin(2*u))*cos(v)", "(2+cos(v/2)*sin(u)-sin(v/2)*sin(2*u))*sin(v)", "sin(v/2)*sin(u)+cos(v/2)*sin(2*u)"}},
							{"Enneper", {"u-u^3/3+u*v^2", "v-v^3/3+v*u^2", "u^2-v^2"}},
							{"Helicoid", {"u*cos(v)", "u*sin(v)", "v"}},
						};
						presetRanges = {
							{0, 6.283f, 0, 3.1416f, 0, 0},   // Sphere: u=0 to 2π, v=0 to π
							{0, 6.283f, 0, 6.283f, 0, 0},    // Torus: u=0 to 2π, v=0 to 2π
							{0, 6.283f, -0.5f, 0.5f, 0, 0},  // Mobius: u=0 to 2π, v=-0.5 to 0.5
							{0, 6.283f, -1.0f, 1.0f, 0, 0},  // Hyperboloid: u=0 to 2π, v=-1 to 1
							{0, 6.283f, 0, 6.283f, 0, 0},    // Klein: u=0 to 2π, v=0 to 2π
							{-2.0f, 2.0f, -2.0f, 2.0f, 0, 0}, // Enneper: u,v=-2 to 2
							{-2.0f, 2.0f, -3.14f, 3.14f, 0, 0}, // Helicoid: u=-2 to 2, v=-π to π
						};
					} else if (n == 2 && m == 1) {
						presets = {
							{"sin(x)*cos(y)", {"sin(x)*cos(y)"}},
							{"Ripple", {"sin(sqrt(x^2+y^2))"}},
							{"Saddle", {"x^2-y^2"}},
							{"Paraboloid", {"x^2+y^2"}},
							{"Gaussian", {"exp(-(x^2+y^2))"}},
							{"Monkey Saddle", {"x^3-3*x*y^2"}},
							{"Valley", {"-(x^2+y^2)"}},
							{"Waves", {"sin(x)+cos(y)"}},
							{"Terrain", {"0.5*sin(2*x)+0.3*cos(3*y)+0.2*sin(x+y)"}},
							{"Egg Carton", {"sin(x)*sin(y)"}},
						};
						presetRanges = {
							{0, 6.283f, 0, 6.283f, 0, 0},    // sin: x,y = 0 to 2π
							{-10, 10, -10, 10, 0, 0},         // Ripple
							{-3, 3, -3, 3, 0, 0},             // Saddle
							{-3, 3, -3, 3, 0, 0},             // Paraboloid
							{-3, 3, -3, 3, 0, 0},             // Gaussian
							{-2, 2, -2, 2, 0, 0},             // Monkey Saddle
							{-3, 3, -3, 3, 0, 0},             // Valley
							{0, 6.283f, 0, 6.283f, 0, 0},    // Waves: x,y = 0 to 2π
							{-5, 5, -5, 5, 0, 0},             // Terrain
							{0, 6.283f, 0, 6.283f, 0, 0},    // Egg Carton: x,y = 0 to 2π
						};
					} else if (n == 2 && m == 2) {
						presets = {
							{"Identity", {"x", "y"}},
							{"Swirl", {"x*cos(y)-y*sin(x)", "x*sin(y)+y*cos(x)"}},
						};
						presetRanges = {
							{-5, 5, -5, 5, 0, 0},
							{-5, 5, -5, 5, 0, 0},
						};
					} else if (n == 3 && m == 3) {
						presets = {
							{"Rotation", {"-y", "x", "0"}},
							{"Spiral", {"-y", "x", "sin(z)"}},
							{"Radial", {"x/sqrt(x^2+y^2+z^2+0.01)", "y/sqrt(x^2+y^2+z^2+0.01)", "z/sqrt(x^2+y^2+z^2+0.01)"}},
							{"Saddle", {"x", "-y", "0"}},
							{"Vortex", {"-y", "x", "-z"}},
							{"Shear", {"y", "0", "0"}},
							{"Lorenz-like", {"10*(y-x)", "x*(28-z)-y", "x*y-8*z/3"}},
							{"Dipole", {"3*x*z/(x^2+y^2+z^2+1)", "3*y*z/(x^2+y^2+z^2+1)", "(2*z^2-x^2-y^2)/(x^2+y^2+z^2+1)"}},
						};
						presetRanges = {
							{-5, 5, -5, 5, -5, 5},        // Rotation
							{-5, 5, -5, 5, -5, 5},        // Spiral
							{-5, 5, -5, 5, -5, 5},        // Radial
							{-5, 5, -5, 5, -5, 5},        // Saddle
							{-5, 5, -5, 5, -5, 5},        // Vortex
							{-5, 5, -5, 5, -5, 5},        // Shear
							{-2, 2, -2, 2, -2, 2},        // Lorenz-like (smaller range)
							{-3, 3, -3, 3, -3, 3},        // Dipole
						};
					} else if (n == 3 && m == 2) {
						presets = {
							{"Rotation 2D", {"-y", "x"}},
							{"Saddle 2D", {"x", "-y"}},
							{"Source 2D", {"x", "y"}},
							{"Sink 2D", {"-x", "-y"}},
							{"Spiral 2D", {"-y+x/5", "x+y/5"}},
						};
						presetRanges = {
							{-5, 5, -5, 5, -5, 5},        // Rotation 2D
							{-5, 5, -5, 5, -5, 5},        // Saddle 2D
							{-5, 5, -5, 5, -5, 5},        // Source 2D
							{-5, 5, -5, 5, -5, 5},        // Sink 2D
							{-5, 5, -5, 5, -5, 5},        // Spiral 2D
						};
					} else if (n == 3 && m == 1) {
						presets = {
							{"Distance", {"sqrt(x^2+y^2+z^2)"}},
							{"Sine Field", {"sin(x)*cos(y)*sin(z)"}},
							{"Gaussian Blob", {"exp(-(x^2+y^2+z^2))"}},
							{"Torus Potential", {"(sqrt(x^2+y^2)-2)^2+z^2"}},
							{"Gyroid", {"sin(x)*cos(y)+sin(y)*cos(z)+sin(z)*cos(x)"}},
							{"Waves 3D", {"sin(x)+sin(y)+sin(z)"}},
							{"Saddle 3D", {"x^2+y^2-z^2"}},
						};
						presetRanges = {
							{-5, 5, -5, 5, -5, 5},        // Distance
							{-5, 5, -5, 5, -5, 5},        // Sine Field
							{-3, 3, -3, 3, -3, 3},        // Gaussian Blob
							{-4, 4, -4, 4, -4, 4},        // Torus Potential
							{-3.14f, 3.14f, -3.14f, 3.14f, -3.14f, 3.14f}, // Gyroid
							{-3.14f, 3.14f, -3.14f, 3.14f, -3.14f, 3.14f}, // Waves 3D
							{-3, 3, -3, 3, -3, 3},        // Saddle 3D
						};
					}

					if (!presets.empty()) {
						std::vector<const char*> presetNames;
						presetNames.push_back("(Custom)");
						for (auto& p : presets) presetNames.push_back(p.first.c_str());

						int presetIdx = 0;
						if (ImGui::Combo("Preset##preset", &presetIdx, presetNames.data(), (int)presetNames.size())) {
							if (presetIdx > 0) {
								auto& p = presets[presetIdx - 1];
								for (int i = 0; i < (int)p.second.size() && i < 3; ++i) {
									fd.exprStrings[i] = p.second[i];
								}
								auto& r = presetRanges[presetIdx - 1];
								fd.rangeMin[0] = r[0]; fd.rangeMax[0] = r[1];
								if (fd.inputDim >= 2) { fd.rangeMin[1] = r[2]; fd.rangeMax[1] = r[3]; }
								if (fd.inputDim >= 3) { fd.rangeMin[2] = r[4]; fd.rangeMax[2] = r[5]; }
								dirty = true;
							}
						}
					}
				}

				// Range controls (drag + expression input)
				if (fd.inputDim == 1) {
					ImGui::Text("%s min", fd.paramNames[0].c_str());
					dirty |= ImGui::DragFloatExpr("##p0min", &fd.rangeMin[0], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s max", fd.paramNames[0].c_str());
					dirty |= ImGui::DragFloatExpr("##p0max", &fd.rangeMax[0], 0.1f, -50.0f, 50.0f);
					ImGui::Text("Segments"); ImGui::SameLine(); dirty |= ImGui::DragInt("##segments", &fd.resolution[0], 1.0f, 10, 500);
					if (fd.outputDim != 2) {
						ImGui::Text("Tube Radius"); ImGui::SameLine(); dirty |= ImGui::DragFloat("##tuberadius", &fd.tubeRadius, 0.001f, 0.005f, 0.2f);
					}
					// For 2D curves, add plane selection
					if (fd.outputDim == 2) {
						ImGui::Text("Plane"); ImGui::SameLine();
						const char* planes[] = {"xy", "xz", "yz"};
						dirty |= ImGui::Combo("##plane", &fd.curvePlane, planes, 3);
					}
				} else if (fd.inputDim == 2) {
					ImGui::Text("%s min", fd.paramNames[0].c_str());
					dirty |= ImGui::DragFloatExpr("##p0min", &fd.rangeMin[0], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s max", fd.paramNames[0].c_str());
					dirty |= ImGui::DragFloatExpr("##p0max", &fd.rangeMax[0], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s min", fd.paramNames[1].c_str());
					dirty |= ImGui::DragFloatExpr("##p1min", &fd.rangeMin[1], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s max", fd.paramNames[1].c_str());
					dirty |= ImGui::DragFloatExpr("##p1max", &fd.rangeMax[1], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s Res", fd.paramNames[0].c_str()); ImGui::SameLine(); dirty |= ImGui::DragInt("##p0res", &fd.resolution[0], 1.0f, 4, 300);
					ImGui::Text("%s Res", fd.paramNames[1].c_str()); ImGui::SameLine(); dirty |= ImGui::DragInt("##p1res", &fd.resolution[1], 1.0f, 4, 300);
				} else if (fd.inputDim == 3) {
					ImGui::Text("%s Min", fd.paramNames[0].c_str());
					dirty |= ImGui::DragFloatExpr("##p0min", &fd.rangeMin[0], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s Max", fd.paramNames[0].c_str());
					dirty |= ImGui::DragFloatExpr("##p0max", &fd.rangeMax[0], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s Min", fd.paramNames[1].c_str());
					dirty |= ImGui::DragFloatExpr("##p1min", &fd.rangeMin[1], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s Max", fd.paramNames[1].c_str());
					dirty |= ImGui::DragFloatExpr("##p1max", &fd.rangeMax[1], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s Min", fd.paramNames[2].c_str());
					dirty |= ImGui::DragFloatExpr("##p2min", &fd.rangeMin[2], 0.1f, -50.0f, 50.0f);
					ImGui::Text("%s Max", fd.paramNames[2].c_str());
					dirty |= ImGui::DragFloatExpr("##p2max", &fd.rangeMax[2], 0.1f, -50.0f, 50.0f);
					ImGui::Text("Resolution"); ImGui::SameLine(); dirty |= ImGui::DragInt("##vfres", &fd.vfResolution, 0.1f, 2, 10);
					if (fd.outputDim >= 2) {
						ImGui::Text("Arrow Scale"); ImGui::SameLine(); dirty |= ImGui::DragFloat("##arrowscale", &fd.arrowScale, 0.01f, 0.05f, 2.0f);
					}
				}

				dirty |= ImGui::ColorEdit3("Color", fd.color);
				ImGui::Text("Opacity"); ImGui::SameLine();
				dirty |= ImGui::SliderFloat("##opacity", &fd.opacity, 0.0f, 1.0f);

				// ── Overlay options ──
				ImGui::Separator();
				ImGui::Text("Overlays:");
				if (fd.inputDim == 1) {
					// Curve overlays
					dirty |= ImGui::Checkbox("Wireframe", &fd.wireframe);
					dirty |= ImGui::Checkbox("Tangent Vector(s)", &fd.showTangentVectors);
					dirty |= ImGui::Checkbox("Normal Vector", &fd.showNormalVectors);
					if (fd.showNormalVectors) {
						ImGui::Indent();
						dirty |= ImGui::Checkbox("Flip Normal Direction", &fd.flipNormalVectors);
						ImGui::Unindent();
					}
					dirty |= ImGui::Checkbox("Frenet Frame (TNB)", &fd.showFrenetFrame);
					if (fd.showFrenetFrame) {
						ImGui::Text("Frame Position"); ImGui::SameLine(); dirty |= ImGui::DragFloat("##frenetpos", &fd.frenetT, 0.01f, 0.0f, 1.0f);
					}
				} else if (fd.inputDim == 2) {
					// Surface overlays
					dirty |= ImGui::Checkbox("Wireframe", &fd.wireframe);
					dirty |= ImGui::Checkbox("Normal Vector", &fd.showNormalVectors);
					if (fd.showNormalVectors) {
						ImGui::Indent();
						dirty |= ImGui::Checkbox("Flip Normal Direction", &fd.flipNormalVectors);
						ImGui::Unindent();
					}
					dirty |= ImGui::Checkbox("Tangent Vector(s)", &fd.showTangentVectors);
					if (fd.showTangentVectors) {
						ImGui::Indent();
						ImGui::Text("Show"); ImGui::SameLine();
						const char* tangentModes[] = {"Both u & v", "u only", "v only"};
						dirty |= ImGui::Combo("##tangentmode", &fd.surfaceTangentMode, tangentModes, 3);
						ImGui::Unindent();
					}
					if (fd.outputDim == 1) {
						dirty |= ImGui::Checkbox("Gradient Field", &fd.showGradientField);
					}
				} else if (fd.inputDim == 3) {
					// Vector/scalar field overlays
					if (fd.outputDim == 1) {
						dirty |= ImGui::Checkbox("Gradient Field", &fd.showGradientField);
					} else {
						dirty |= ImGui::Checkbox("Vector Field", &fd.showVectorField);
						dirty |= ImGui::Checkbox("Streamlines", &fd.showStreamlines);
					}
				}
				if (fd.showTangentVectors || fd.showNormalVectors || fd.showFrenetFrame || fd.showGradientField || fd.showVectorField || fd.showStreamlines) {
					ImGui::Text("Overlay Count"); ImGui::SameLine(); dirty |= ImGui::DragInt("##overlaycount", &fd.overlayVectorCount, 0.5f, 2, 100);
					ImGui::Text("Overlay Scale"); ImGui::SameLine(); dirty |= ImGui::DragFloat("##overlayscale", &fd.overlayVectorScale, 0.01f, 0.05f, 1.5f);
				}

				if (ImGui::Button("Remove")) {
					removeIdx = fi;
				}
			}

			ImGui::PopID();
			ImGui::Separator();
		}

		// Remove function if requested
		if (removeIdx >= 0 && removeIdx < (int)m_functions.size()) {
			m_functions.erase(m_functions.begin() + removeIdx);
			dirty = true;
		}

		// Add function button
		if (ImGui::Button("+ Add Function")) {
			FunctionDefinition newFd;
			newFd.name = "f";
			newFd.inputDim = 1;
			newFd.outputDim = 3;
			newFd.paramNames[0] = "t";
			newFd.exprStrings[0] = "cos(t)";
			newFd.exprStrings[1] = "sin(t)";
			newFd.exprStrings[2] = "0";
			// Use default range -10 to 10 (from FunctionDefinition struct)
			newFd.resolution[0] = 200;
			compileFunctionDef(newFd);
			m_functions.push_back(std::move(newFd));
			dirty = true;
		}

		ImGui::PopItemWidth();
		ImGui::End(); // Visualization

		// Recompile all expressions when dirty
		if (dirty) {
			for (auto& fd : m_functions) {
				compileFunctionDef(fd);
			}
			m_graphObjectsDirty = true;
		}

		// ── Settings window ──
		ImGui::Begin("Settings");
		ImGui::PushItemWidth(150);

		// ── Axes & Grid ──
		if (ImGui::CollapsingHeader("Axes & Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
			bool axesDirty = false;
			axesDirty |= ImGui::Checkbox("Show Axes", &m_showAxes);
			axesDirty |= ImGui::Checkbox("Show Grid", &m_showGrid);
			if (m_showGrid) {
				axesDirty |= ImGui::Checkbox("XY Plane", &m_showXYGrid);
				ImGui::SameLine();
				axesDirty |= ImGui::Checkbox("XZ Plane", &m_showXZGrid);
				ImGui::SameLine();
				axesDirty |= ImGui::Checkbox("YZ Plane", &m_showYZGrid);
				ImGui::Text("Grid Spacing"); ImGui::SameLine(); axesDirty |= ImGui::DragFloat("##gridspacing", &m_gridSpacing, 0.05f, 0.25f, 5.0f);
				ImGui::Text("Grid Extent"); ImGui::SameLine(); axesDirty |= ImGui::DragFloat("##gridextent", &m_gridExtent, 0.5f, 1.0f, 50.0f);
				axesDirty |= ImGui::ColorEdit3("Grid Color", m_gridColor);
			}
			axesDirty |= ImGui::ColorEdit3("X Axis", m_axisColors[0]);
			axesDirty |= ImGui::ColorEdit3("Y Axis", m_axisColors[1]);
			axesDirty |= ImGui::ColorEdit3("Z Axis", m_axisColors[2]);
			ImGui::ColorEdit3("Background", m_bgColor);
			if (axesDirty) m_axesDirty = true;
		}

		ImGui::Separator();

		// ── Display ──
		if (ImGui::CollapsingHeader("Display")) {
			ImGui::Checkbox("Show Boat", &m_showBoat);
		}

		ImGui::Separator();

		// ── Lighting ──
		if (ImGui::CollapsingHeader("Lighting")) {
			lightingChanged |= ImGui::ColorEdit3("Color #0", glm::value_ptr(m_lightingUniforms.colors[0]));
			lightingChanged |= ImGui::DragDirection("Direction #0", m_lightingUniforms.directions[0]);
			lightingChanged |= ImGui::ColorEdit3("Color #1", glm::value_ptr(m_lightingUniforms.colors[1]));
			lightingChanged |= ImGui::DragDirection("Direction #1", m_lightingUniforms.directions[1]);
			ImGui::Text("Hardness"); ImGui::SameLine(); lightingChanged |= ImGui::DragFloat("##hardness", &m_lightingUniforms.hardness, 1.0f, 1.0f, 100.0f);
			ImGui::Text("K Diffuse"); ImGui::SameLine(); lightingChanged |= ImGui::DragFloat("##kdiffuse", &m_lightingUniforms.kd, 0.01f, 0.0f, 1.0f);
			ImGui::Text("K Specular"); ImGui::SameLine(); lightingChanged |= ImGui::DragFloat("##kspecular", &m_lightingUniforms.ks, 0.01f, 0.0f, 1.0f);
		}

		ImGui::PopItemWidth();
		ImGui::End(); // Settings

		m_lightingUniformsChanged = lightingChanged;

		// ── Function label overlay (top-right) ──
		{
			ImGuiIO& io = ImGui::GetIO();
			float padding = 10.0f;
			float yOffset = padding;

			for (int fi = 0; fi < (int)m_functions.size(); ++fi) {
				auto& fd = m_functions[fi];
				if (!fd.show || !fd.isValid) continue;

				// Build params string: "t" or "u,v" or "x,y,z"
				std::string params;
				for (int i = 0; i < fd.inputDim; ++i) {
					if (i > 0) params += ",";
					params += fd.paramNames[i];
				}

				// Build type label with R^n -> R^m mapping
				std::string typeLabel;
				if (fd.inputDim == 1) typeLabel = "Curve";
				else if (fd.inputDim == 2) typeLabel = "Surface";
				else if (fd.inputDim == 3 && fd.outputDim == 1) typeLabel = "Scalar Field";
				else typeLabel = "Vector Field";
				typeLabel += "  " + fd.name + ": R" + (fd.inputDim > 1 ? "^" + std::to_string(fd.inputDim) : "")
					+ " -> R" + (fd.outputDim > 1 ? "^" + std::to_string(fd.outputDim) : "") + ",  ";

				// Build expression string: "r(t) = (cos(t), sin(t), t/(2*pi))"
				std::string exprText = fd.name + "(" + params + ") = ";
				if (fd.outputDim == 1) {
					exprText += fd.exprStrings[0];
				} else {
					exprText += "(";
					for (int i = 0; i < fd.outputDim; ++i) {
						if (i > 0) exprText += ", ";
						exprText += fd.exprStrings[i];
					}
					exprText += ")";
				}

				std::string label = typeLabel + exprText;

				// Static map to track expanded state per function
				static std::map<int, bool> expandedStates;
				bool& expanded = expandedStates[fi];

				// Measure text size to position from top-right
				ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
				float windowWidth = textSize.x + padding * 2;

				ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - windowWidth - padding, yOffset), ImGuiCond_Always);
				ImGui::SetNextWindowSize(ImVec2(0, 0));
				ImGui::SetNextWindowBgAlpha(0.7f);

				std::string overlayId = "##overlay" + std::to_string(fi);
				ImGui::Begin(overlayId.c_str(), nullptr,
					ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
					ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(fd.color[0], fd.color[1], fd.color[2], 1.0f));

				// Main label is clickable
				if (ImGui::Selectable(label.c_str(), expanded, ImGuiSelectableFlags_None)) {
					expanded = !expanded;
				}

				// Show details when expanded
				if (expanded) {
					ImGui::Separator();
					ImGui::Text("Range:");
					if (fd.inputDim >= 1) {
						ImGui::Text("  %s: [%.2f, %.2f]", fd.paramNames[0].c_str(), fd.rangeMin[0], fd.rangeMax[0]);
					}
					if (fd.inputDim >= 2) {
						ImGui::Text("  %s: [%.2f, %.2f]", fd.paramNames[1].c_str(), fd.rangeMin[1], fd.rangeMax[1]);
					}
					if (fd.inputDim >= 3) {
						ImGui::Text("  %s: [%.2f, %.2f]", fd.paramNames[2].c_str(), fd.rangeMin[2], fd.rangeMax[2]);
					}
					ImGui::Text("Resolution:");
					if (fd.inputDim == 1) {
						ImGui::Text("  Segments: %d", fd.resolution[0]);
					} else if (fd.inputDim == 2) {
						ImGui::Text("  U: %d, V: %d", fd.resolution[0], fd.resolution[1]);
					}
				}

				ImGui::PopStyleColor();

				yOffset += ImGui::GetWindowHeight() + 4.0f;
				ImGui::End();
			}
		}
	}

	// Draw the UI
	ImGui::EndFrame();
	// Convert the UI defined above into low-level drawing commands
	ImGui::Render();
	// Execute the low-level drawing commands on the WebGPU backend
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}
