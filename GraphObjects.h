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

	// Map a value in [0,1] to a blue->green->red gradient (public for scalar field)
	static vec3 magnitudeToColor(float t);

private:
	// Map height to color based on min/max range
	static vec3 heightToColor(float height, float minH, float maxH);
};
