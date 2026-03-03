#version 450
#include "common.glsl"

layout(location = 0) in vec2 inNDC; 

layout(location = 0) out vec4 outColor;

// 2D 그리드를 그리는 헬퍼 함수
vec4 grid2D(vec2 coord, float scale, float thickness) {
    vec2 scaledCoord = coord * scale;
    vec2 derivative = fwidth(scaledCoord);
    vec2 grid = abs(fract(scaledCoord - 0.5) - 0.5) / derivative;
    
    float line = min(grid.x, grid.y) / thickness;
    
    return vec4(0.2, 0.2, 0.2, 0.5 - min(line, 0.5));
}

void main() {
    // --- 아래부터는 화면 왼쪽 절반(inNDC.x <= 0.0)에만 실행됩니다. ---

    // 1. NDC 화면 좌표를 2D World 좌표(XY)로 역투영 (Unproject)
    mat4 invViewProj = inverse(commonFragdata.viewProjection);
    vec4 worldPos = invViewProj * vec4(inNDC, 0.0, 1.0);
    vec2 coord = worldPos.xy;

    // 2. 1m 단위의 얇은 선 (두께 1.0)
    vec4 grid1 = grid2D(coord, 1.0, 1.0);
    
    // 3. 10m 단위의 굵은 선 (두께 3.0)
    vec4 grid10 = grid2D(coord, 0.1, 3.0);
    
    // 4. 선 합성
    vec4 color = grid1 + grid10;
    color.a = max(grid1.a, grid10.a);

    // 5. 중심 축(Axis) 색칠하기
    vec2 derivative = fwidth(coord);
    float minX = min(derivative.x, 1.0);
    float minY = min(derivative.y, 1.0);
    
    // X축 (가로선, Y=0) - 빨간색
    if (abs(coord.y) < 0.1 * minY * 3.0) color.rgb = vec3(1.0, 0.2, 0.2);
    // Y축 (세로선, X=0) - 초록색
    if (abs(coord.x) < 0.1 * minX * 3.0) color.rgb = vec3(0.2, 1.0, 0.2);

    outColor = color;
    
    // 투명한 픽셀은 과감히 버려서 렌더링 부하 최소화
    // 페이드 효과를 위해 기준값을 조금 낮춰줍니다.
    if (outColor.a <= 0.05) discard;
}