#pragma once

#include "ResourceManager.h"
#include <glm/glm.hpp>
#include <functional>
#include <vector>

class GraphObjects {
public:
	using VertexAttributes = ResourceManager::VertexAttributes;
	using vec3 = glm::vec3;
	using ivec3 = glm::ivec3;

	// Generate a single 3D arrow mesh (cone+cylinder) along +Z, at origin
	static std::vector<VertexAttributes> generateArrowMesh(
		float shaftLength, float shaftRadius, float headLength, float headRadius,
		int segments = 8, vec3 color = vec3(1, 0, 0));

	// Generate a full vector field: arrows at grid sample points
	static std::vector<VertexAttributes> generateVectorField(
		std::function<vec3(vec3)> fieldFunc,
		vec3 rangeMin, vec3 rangeMax, ivec3 resolution,
		float arrowScale = 1.0f);

	// Generate a parametric curve r(t) = (x(t), y(t), z(t))
	// Returns LineList vertices (pairs of endpoints)
	static std::vector<VertexAttributes> generateParametricCurve(
		std::function<vec3(float)> curveFunc,
		float tMin, float tMax, int segments,
		vec3 color = vec3(1, 1, 0));

	// Generate a parametric curve as a tube mesh (TriangleList) for lit rendering
	static std::vector<VertexAttributes> generateParametricCurveTube(
		std::function<vec3(float)> curveFunc,
		float tMin, float tMax, int segments,
		float tubeRadius = 0.03f, int tubeSegments = 8,
		vec3 color = vec3(1, 1, 0));

	// Generate a parametric surface r(u,v)
	// Returns TriangleList vertices with normals for Blinn-Phong
	static std::vector<VertexAttributes> generateParametricSurface(
		std::function<vec3(float, float)> surfaceFunc,
		float uMin, float uMax, float vMin, float vMax,
		int uSegments, int vSegments,
		bool colorByHeight = true);

	// Generate a scalar field visualization: small colored cubes at grid points
	static std::vector<VertexAttributes> generateScalarField(
		std::function<float(vec3)> scalarFunc,
		vec3 rangeMin, vec3 rangeMax, ivec3 resolution,
		float cubeSize = 0.1f);

	// Generate a small colored cube centered at origin (12 triangles = 36 verts)
	static std::vector<VertexAttributes> generateColoredCube(
		float halfSize, vec3 color);

	// Generate a parametric surface as wireframe (LineList)
	static std::vector<VertexAttributes> generateParametricSurfaceWireframe(
		std::function<vec3(float, float)> surfaceFunc,
		float uMin, float uMax, float vMin, float vMax,
		int uSegments, int vSegments,
		vec3 color = vec3(1, 1, 1));

	// Generate tangent vector arrows along a parametric curve
	static std::vector<VertexAttributes> generateTangentVectors(
		std::function<vec3(float)> curveFunc,
		float tMin, float tMax, int count,
		float arrowScale = 0.3f, vec3 color = vec3(1, 0, 0));

	// Generate normal vector arrows on a parametric surface
	static std::vector<VertexAttributes> generateSurfaceNormals(
		std::function<vec3(float, float)> surfaceFunc,
		float uMin, float uMax, float vMin, float vMax,
		int uCount, int vCount,
		float arrowScale = 0.3f, vec3 color = vec3(0, 0, 1), bool flipNormal = false);

	// Generate Frenet frame (T/N/B) at a single point on a curve
	static std::vector<VertexAttributes> generateFrenetFrame(
		std::function<vec3(float)> curveFunc,
		float tMin, float tMax, float tNorm,
		float arrowScale = 0.5f);

	// Generate gradient field arrows for a scalar function R^2->R^1
	static std::vector<VertexAttributes> generateGradientField2D(
		std::function<float(float, float)> scalarFunc,
		float uMin, float uMax, float vMin, float vMax,
		int uCount, int vCount,
		float arrowScale = 0.3f);

	// Generate gradient field arrows for a scalar function R^3->R^1
	static std::vector<VertexAttributes> generateGradientField3D(
		std::function<float(vec3)> scalarFunc,
		vec3 rangeMin, vec3 rangeMax, ivec3 resolution,
		float arrowScale = 0.3f);

	// Map a value in [0,1] to a blue->green->red gradient (public for scalar field)
	static vec3 magnitudeToColor(float t);

private:
	// Map height to color based on min/max range
	static vec3 heightToColor(float height, float minH, float maxH);
};
