struct Camera {
    viewProjection: mat4x4f,
};

@group(0) @binding(0) var<uniform> camera: Camera;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
};

@vertex
fn vs(@location(0) position: vec3f, @location(1) shade: f32,
      @location(2) column0: vec4f, @location(3) column1: vec4f,
      @location(4) column2: vec4f, @location(5) column3: vec4f,
      @location(6) color: vec3f) -> VertexOutput {
    let world = mat4x4f(column0, column1, column2, column3);
    return VertexOutput(camera.viewProjection * world * vec4f(position, 1), color * shade);
}

@fragment
fn fs(input: VertexOutput) -> @location(0) vec4f {
    return vec4f(input.color, 1);
}
