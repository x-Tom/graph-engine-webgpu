struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) viewDirection: vec3<f32>,
};

struct MyUniforms {
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
	modelMatrix: mat4x4f,
	color: vec4f,
	cameraWorldPosition: vec3f,
	time: f32,
};

struct LightingUniforms {
	directions: array<vec4f, 2>,
	colors: array<vec4f, 2>,
	hardness: f32,
	kd: f32,
	ks: f32,
}

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;
@group(0) @binding(1) var baseColorTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;
@group(0) @binding(3) var<uniform> uLighting: LightingUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	let worldPosition = uMyUniforms.modelMatrix * vec4<f32>(in.position, 1.0);
	out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * worldPosition;
	out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.color = in.color;
	out.uv = in.uv;
	out.viewDirection = uMyUniforms.cameraWorldPosition - worldPosition.xyz;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	var N = normalize(in.normal);
	let V = normalize(in.viewDirection);

	// Two-sided lighting: flip normal for back-faces
	if (dot(N, V) < 0.0) {
		N = -N;
	}

	let baseColor = in.color;
	let kd = uLighting.kd;
	let ks = uLighting.ks;
	let hardness = uLighting.hardness;

	// Ambient
	var color = vec3f(0.05) * baseColor;

	for (var i: i32 = 0; i < 2; i++) {
		let lightColor = uLighting.colors[i].rgb;
		let L = normalize(uLighting.directions[i].xyz);
		let R = reflect(-L, N);

		let diffuse = max(0.0, dot(L, N)) * lightColor;
		let RoV = max(0.0, dot(R, V));
		let specular = pow(RoV, hardness);

		color += baseColor * kd * diffuse + ks * specular;
	}

	return vec4f(color, 1.0);
}
