#include "GraphObjects.h"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

using vec3 = glm::vec3;
using VertexAttributes = ResourceManager::VertexAttributes;

constexpr float GPI = 3.14159265358979323846f;

// ─── Color Utilities ────────────────────────────────────────────────────────

vec3 GraphObjects::magnitudeToColor(float t) {
	t = glm::clamp(t, 0.0f, 1.0f);
	// Blue -> Cyan -> Green -> Yellow -> Red
	if (t < 0.25f) {
		float s = t / 0.25f;
		return vec3(0, s, 1);
	} else if (t < 0.5f) {
		float s = (t - 0.25f) / 0.25f;
		return vec3(0, 1, 1 - s);
	} else if (t < 0.75f) {
		float s = (t - 0.5f) / 0.25f;
		return vec3(s, 1, 0);
	} else {
		float s = (t - 0.75f) / 0.25f;
		return vec3(1, 1 - s, 0);
	}
}

vec3 GraphObjects::heightToColor(float height, float minH, float maxH) {
	if (maxH - minH < 1e-6f) return vec3(0.5f, 0.7f, 1.0f);
	float t = (height - minH) / (maxH - minH);
	return magnitudeToColor(t);
}

// ─── Arrow Mesh ─────────────────────────────────────────────────────────────

std::vector<VertexAttributes> GraphObjects::generateArrowMesh(
	float shaftLength, float shaftRadius, float headLength, float headRadius,
	int segments, vec3 color) {

	std::vector<VertexAttributes> verts;

	// Cylinder (shaft) from z=0 to z=shaftLength
	for (int i = 0; i < segments; ++i) {
		float a0 = 2.0f * GPI * i / segments;
		float a1 = 2.0f * GPI * (i + 1) / segments;

		float c0 = cosf(a0), s0 = sinf(a0);
		float c1 = cosf(a1), s1 = sinf(a1);

		vec3 n0(c0, s0, 0), n1(c1, s1, 0);

		vec3 p00(shaftRadius * c0, shaftRadius * s0, 0);
		vec3 p01(shaftRadius * c1, shaftRadius * s1, 0);
		vec3 p10(shaftRadius * c0, shaftRadius * s0, shaftLength);
		vec3 p11(shaftRadius * c1, shaftRadius * s1, shaftLength);

		// Two triangles per quad
		verts.push_back({p00, n0, color, {0, 0}});
		verts.push_back({p01, n1, color, {0, 0}});
		verts.push_back({p10, n0, color, {0, 0}});

		verts.push_back({p10, n0, color, {0, 0}});
		verts.push_back({p01, n1, color, {0, 0}});
		verts.push_back({p11, n1, color, {0, 0}});
	}

	// Cone (head) from z=shaftLength to z=shaftLength+headLength
	vec3 tip(0, 0, shaftLength + headLength);
	float hyp = sqrtf(headRadius * headRadius + headLength * headLength);
	float nr = headLength / hyp; // radial component of normal
	float nz = headRadius / hyp; // z component of normal

	for (int i = 0; i < segments; ++i) {
		float a0 = 2.0f * GPI * i / segments;
		float a1 = 2.0f * GPI * (i + 1) / segments;

		float c0 = cosf(a0), s0 = sinf(a0);
		float c1 = cosf(a1), s1 = sinf(a1);

		vec3 base0(headRadius * c0, headRadius * s0, shaftLength);
		vec3 base1(headRadius * c1, headRadius * s1, shaftLength);

		vec3 n0(nr * c0, nr * s0, nz);
		vec3 n1(nr * c1, nr * s1, nz);
		vec3 nTip = glm::normalize(n0 + n1);

		verts.push_back({base0, n0, color, {0, 0}});
		verts.push_back({base1, n1, color, {0, 0}});
		verts.push_back({tip, nTip, color, {0, 0}});
	}

	// Bottom cap of cylinder (disc at z=0)
	vec3 capNormal(0, 0, -1);
	for (int i = 0; i < segments; ++i) {
		float a0 = 2.0f * GPI * i / segments;
		float a1 = 2.0f * GPI * (i + 1) / segments;

		vec3 p0(shaftRadius * cosf(a0), shaftRadius * sinf(a0), 0);
		vec3 p1(shaftRadius * cosf(a1), shaftRadius * sinf(a1), 0);

		verts.push_back({{0, 0, 0}, capNormal, color, {0, 0}});
		verts.push_back({p1, capNormal, color, {0, 0}});
		verts.push_back({p0, capNormal, color, {0, 0}});
	}

	// Bottom cap of cone (disc at z=shaftLength)
	vec3 coneCapN(0, 0, -1);
	for (int i = 0; i < segments; ++i) {
		float a0 = 2.0f * GPI * i / segments;
		float a1 = 2.0f * GPI * (i + 1) / segments;

		vec3 p0(headRadius * cosf(a0), headRadius * sinf(a0), shaftLength);
		vec3 p1(headRadius * cosf(a1), headRadius * sinf(a1), shaftLength);

		verts.push_back({{0, 0, shaftLength}, coneCapN, color, {0, 0}});
		verts.push_back({p1, coneCapN, color, {0, 0}});
		verts.push_back({p0, coneCapN, color, {0, 0}});
	}

	return verts;
}

// ─── Vector Field ───────────────────────────────────────────────────────────

std::vector<VertexAttributes> GraphObjects::generateVectorField(
	std::function<vec3(vec3)> fieldFunc,
	vec3 rangeMin, vec3 rangeMax, ivec3 resolution,
	float arrowScale) {

	std::vector<VertexAttributes> allVerts;

	vec3 step = (rangeMax - rangeMin) / vec3(
		std::max(resolution.x - 1, 1),
		std::max(resolution.y - 1, 1),
		std::max(resolution.z - 1, 1)
	);

	// First pass: evaluate field and find max magnitude
	struct ArrowInfo { vec3 pos; vec3 dir; float mag; };
	std::vector<ArrowInfo> arrows;
	float maxMag = 0.001f;

	for (int ix = 0; ix < resolution.x; ++ix) {
		for (int iy = 0; iy < resolution.y; ++iy) {
			for (int iz = 0; iz < resolution.z; ++iz) {
				vec3 pos = rangeMin + vec3(ix, iy, iz) * step;
				vec3 dir = fieldFunc(pos);
				float mag = glm::length(dir);
				maxMag = std::max(maxMag, mag);
				arrows.push_back({pos, dir, mag});
			}
		}
	}

	const float minLen = 0.05f * arrowScale;
	const float maxLen = 0.8f * arrowScale;

	for (auto& arrow : arrows) {
		if (arrow.mag < 1e-6f) continue;

		float normalizedMag = arrow.mag / maxMag;
		float len = minLen + (maxLen - minLen) * normalizedMag;
		vec3 color = magnitudeToColor(normalizedMag);

		// Generate arrow mesh along +Z
		auto mesh = generateArrowMesh(
			len * 0.7f,   // shaft length
			len * 0.04f,  // shaft radius
			len * 0.3f,   // head length
			len * 0.1f,   // head radius
			6, color);

		// Build rotation from +Z to arrow direction
		vec3 dir = glm::normalize(arrow.dir);
		vec3 ref = (std::abs(dir.y) < 0.99f) ? vec3(0, 1, 0) : vec3(1, 0, 0);
		vec3 xAxis = glm::normalize(glm::cross(ref, dir));
		vec3 yAxis = glm::cross(dir, xAxis);

		for (auto& v : mesh) {
			vec3 rotPos = xAxis * v.position.x + yAxis * v.position.y + dir * v.position.z;
			vec3 rotNorm = xAxis * v.normal.x + yAxis * v.normal.y + dir * v.normal.z;
			v.position = rotPos + arrow.pos;
			v.normal = rotNorm;
		}

		allVerts.insert(allVerts.end(), mesh.begin(), mesh.end());
	}

	return allVerts;
}

// ─── Parametric Curve ───────────────────────────────────────────────────────

std::vector<VertexAttributes> GraphObjects::generateParametricCurve(
	std::function<vec3(float)> curveFunc,
	float tMin, float tMax, int segments,
	vec3 color) {

	std::vector<VertexAttributes> verts;
	float dt = (tMax - tMin) / segments;

	for (int i = 0; i < segments; ++i) {
		float t0 = tMin + i * dt;
		float t1 = tMin + (i + 1) * dt;

		vec3 p0 = curveFunc(t0);
		vec3 p1 = curveFunc(t1);

		verts.push_back({p0, {0, 0, 0}, color, {0, 0}});
		verts.push_back({p1, {0, 0, 0}, color, {0, 0}});
	}

	return verts;
}

// ─── Parametric Curve Tube ──────────────────────────────────────────────────

std::vector<VertexAttributes> GraphObjects::generateParametricCurveTube(
	std::function<vec3(float)> curveFunc,
	float tMin, float tMax, int segments,
	float tubeRadius, int tubeSegments,
	vec3 color) {

	std::vector<VertexAttributes> verts;
	if (segments < 1) return verts;

	float dt = (tMax - tMin) / segments;

	// Sample curve points
	std::vector<vec3> points(segments + 1);
	for (int i = 0; i <= segments; ++i) {
		points[i] = curveFunc(tMin + i * dt);
	}

	// Compute tangents via finite differences
	std::vector<vec3> tangents(segments + 1);
	for (int i = 0; i <= segments; ++i) {
		vec3 fwd, bwd;
		if (i < segments) fwd = points[i + 1] - points[i];
		else fwd = points[i] - points[i - 1];
		if (i > 0) bwd = points[i] - points[i - 1];
		else bwd = fwd;
		tangents[i] = glm::normalize((fwd + bwd) * 0.5f);
	}

	// Build rotation-minimizing frames
	std::vector<vec3> normals(segments + 1);
	std::vector<vec3> binormals(segments + 1);

	// Initial frame: find a vector not parallel to first tangent
	vec3 t0 = tangents[0];
	vec3 ref = (std::abs(t0.y) < 0.9f) ? vec3(0, 1, 0) : vec3(1, 0, 0);
	normals[0] = glm::normalize(glm::cross(t0, ref));
	binormals[0] = glm::cross(t0, normals[0]);

	// Propagate frame along curve (rotation-minimizing)
	for (int i = 1; i <= segments; ++i) {
		vec3 prevN = normals[i - 1];
		vec3 prevB = binormals[i - 1];
		vec3 ti = tangents[i];

		// Project previous normal onto plane perpendicular to new tangent
		vec3 projected = prevN - glm::dot(prevN, ti) * ti;
		float len = glm::length(projected);
		if (len > 1e-8f) {
			normals[i] = projected / len;
		} else {
			// Degenerate case: use previous binormal
			normals[i] = prevB;
		}
		binormals[i] = glm::cross(ti, normals[i]);
	}

	// Generate tube rings and connect with triangles
	// Each ring has tubeSegments vertices
	auto ringPoint = [&](int curveIdx, int ringIdx) -> vec3 {
		float angle = 2.0f * GPI * ringIdx / tubeSegments;
		float c = cosf(angle), s = sinf(angle);
		return points[curveIdx] + tubeRadius * (c * normals[curveIdx] + s * binormals[curveIdx]);
	};

	auto ringNormal = [&](int curveIdx, int ringIdx) -> vec3 {
		float angle = 2.0f * GPI * ringIdx / tubeSegments;
		float c = cosf(angle), s = sinf(angle);
		return glm::normalize(c * normals[curveIdx] + s * binormals[curveIdx]);
	};

	for (int i = 0; i < segments; ++i) {
		for (int j = 0; j < tubeSegments; ++j) {
			int j1 = (j + 1) % tubeSegments;

			vec3 p00 = ringPoint(i, j);
			vec3 p01 = ringPoint(i, j1);
			vec3 p10 = ringPoint(i + 1, j);
			vec3 p11 = ringPoint(i + 1, j1);

			vec3 n00 = ringNormal(i, j);
			vec3 n01 = ringNormal(i, j1);
			vec3 n10 = ringNormal(i + 1, j);
			vec3 n11 = ringNormal(i + 1, j1);

			// Triangle 1
			verts.push_back({p00, n00, color, {0, 0}});
			verts.push_back({p01, n01, color, {0, 0}});
			verts.push_back({p10, n10, color, {0, 0}});

			// Triangle 2
			verts.push_back({p10, n10, color, {0, 0}});
			verts.push_back({p01, n01, color, {0, 0}});
			verts.push_back({p11, n11, color, {0, 0}});
		}
	}

	return verts;
}

// ─── Parametric Surface ─────────────────────────────────────────────────────

std::vector<VertexAttributes> GraphObjects::generateParametricSurface(
	std::function<vec3(float, float)> surfaceFunc,
	float uMin, float uMax, float vMin, float vMax,
	int uSegments, int vSegments,
	bool colorByHeight) {

	float du = (uMax - uMin) / uSegments;
	float dv = (vMax - vMin) / vSegments;

	// Evaluate positions and find height range
	std::vector<std::vector<vec3>> positions(uSegments + 1, std::vector<vec3>(vSegments + 1));
	float minH = 1e9f, maxH = -1e9f;

	for (int i = 0; i <= uSegments; ++i) {
		for (int j = 0; j <= vSegments; ++j) {
			float u = uMin + i * du;
			float v = vMin + j * dv;
			positions[i][j] = surfaceFunc(u, v);
			float h = positions[i][j].z;
			minH = std::min(minH, h);
			maxH = std::max(maxH, h);
		}
	}

	// Compute normals via finite differences
	std::vector<std::vector<vec3>> normals(uSegments + 1, std::vector<vec3>(vSegments + 1));
	float eps = 1e-4f;

	for (int i = 0; i <= uSegments; ++i) {
		for (int j = 0; j <= vSegments; ++j) {
			float u = uMin + i * du;
			float v = vMin + j * dv;

			vec3 dpdu = (surfaceFunc(u + eps, v) - surfaceFunc(u - eps, v)) / (2.0f * eps);
			vec3 dpdv = (surfaceFunc(u, v + eps) - surfaceFunc(u, v - eps)) / (2.0f * eps);

			vec3 n = glm::cross(dpdu, dpdv);
			float len = glm::length(n);
			if (len > 1e-8f) n /= len;
			else n = vec3(0, 0, 1);

			normals[i][j] = n;
		}
	}

	// Build triangle list
	std::vector<VertexAttributes> verts;

	for (int i = 0; i < uSegments; ++i) {
		for (int j = 0; j < vSegments; ++j) {
			vec3 p00 = positions[i][j];
			vec3 p10 = positions[i + 1][j];
			vec3 p01 = positions[i][j + 1];
			vec3 p11 = positions[i + 1][j + 1];

			vec3 n00 = normals[i][j];
			vec3 n10 = normals[i + 1][j];
			vec3 n01 = normals[i][j + 1];
			vec3 n11 = normals[i + 1][j + 1];

			vec3 c00 = colorByHeight ? heightToColor(p00.z, minH, maxH) : vec3(0.5f, 0.7f, 1.0f);
			vec3 c10 = colorByHeight ? heightToColor(p10.z, minH, maxH) : vec3(0.5f, 0.7f, 1.0f);
			vec3 c01 = colorByHeight ? heightToColor(p01.z, minH, maxH) : vec3(0.5f, 0.7f, 1.0f);
			vec3 c11 = colorByHeight ? heightToColor(p11.z, minH, maxH) : vec3(0.5f, 0.7f, 1.0f);

			// Triangle 1
			verts.push_back({p00, n00, c00, {0, 0}});
			verts.push_back({p10, n10, c10, {0, 0}});
			verts.push_back({p11, n11, c11, {0, 0}});

			// Triangle 2
			verts.push_back({p00, n00, c00, {0, 0}});
			verts.push_back({p11, n11, c11, {0, 0}});
			verts.push_back({p01, n01, c01, {0, 0}});
		}
	}

	return verts;
}

// ─── Colored Cube ───────────────────────────────────────────────────────────

std::vector<VertexAttributes> GraphObjects::generateColoredCube(float halfSize, vec3 color) {
	std::vector<VertexAttributes> verts;
	float s = halfSize;

	// 6 faces, 2 triangles each = 36 vertices
	// Each face: normal, 4 corner positions
	vec3 normals[6] = {
		vec3(0,0,1), vec3(0,0,-1), vec3(1,0,0),
		vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0)
	};
	vec3 corners[6][4] = {
		{ vec3(-s,-s, s), vec3( s,-s, s), vec3( s, s, s), vec3(-s, s, s) },
		{ vec3(-s,-s,-s), vec3(-s, s,-s), vec3( s, s,-s), vec3( s,-s,-s) },
		{ vec3( s,-s,-s), vec3( s, s,-s), vec3( s, s, s), vec3( s,-s, s) },
		{ vec3(-s,-s,-s), vec3(-s,-s, s), vec3(-s, s, s), vec3(-s, s,-s) },
		{ vec3(-s, s,-s), vec3(-s, s, s), vec3( s, s, s), vec3( s, s,-s) },
		{ vec3(-s,-s,-s), vec3( s,-s,-s), vec3( s,-s, s), vec3(-s,-s, s) },
	};

	for (int f = 0; f < 6; ++f) {
		vec3 n = normals[f];
		verts.push_back({corners[f][0], n, color, {0, 0}});
		verts.push_back({corners[f][1], n, color, {0, 0}});
		verts.push_back({corners[f][2], n, color, {0, 0}});

		verts.push_back({corners[f][0], n, color, {0, 0}});
		verts.push_back({corners[f][2], n, color, {0, 0}});
		verts.push_back({corners[f][3], n, color, {0, 0}});
	}

	return verts;
}

// ─── Scalar Field ───────────────────────────────────────────────────────────

std::vector<VertexAttributes> GraphObjects::generateScalarField(
	std::function<float(vec3)> scalarFunc,
	vec3 rangeMin, vec3 rangeMax, ivec3 resolution,
	float cubeSize) {

	std::vector<VertexAttributes> allVerts;

	vec3 step = (rangeMax - rangeMin) / vec3(
		std::max(resolution.x - 1, 1),
		std::max(resolution.y - 1, 1),
		std::max(resolution.z - 1, 1)
	);

	// First pass: evaluate and find min/max
	struct SampleInfo { vec3 pos; float val; };
	std::vector<SampleInfo> samples;
	float minVal = 1e9f, maxVal = -1e9f;

	for (int ix = 0; ix < resolution.x; ++ix) {
		for (int iy = 0; iy < resolution.y; ++iy) {
			for (int iz = 0; iz < resolution.z; ++iz) {
				vec3 pos = rangeMin + vec3(ix, iy, iz) * step;
				float val = scalarFunc(pos);
				minVal = std::min(minVal, val);
				maxVal = std::max(maxVal, val);
				samples.push_back({pos, val});
			}
		}
	}

	float range = maxVal - minVal;
	if (range < 1e-6f) range = 1.0f;

	float halfSize = cubeSize * 0.5f;

	for (auto& sample : samples) {
		float normalized = (sample.val - minVal) / range;
		vec3 color = magnitudeToColor(normalized);
		auto cube = generateColoredCube(halfSize, color);

		// Translate cube to sample position
		for (auto& v : cube) {
			v.position += sample.pos;
		}

		allVerts.insert(allVerts.end(), cube.begin(), cube.end());
	}

	return allVerts;
}
