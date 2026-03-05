#version 450
#include "common.glsl"

layout(location = 0) in vec2 inNDC; 

layout(location = 0) out vec4 outColor;

// 2D 그리드를 그리는 헬퍼 함수 (안티앨리어싱 및 LOD 페이드 적용)
vec4 grid2D(vec2 coord, float scale, float thickness) {
    vec2 scaledCoord = coord * scale;
    
    // fwidth는 1픽셀당 월드 좌표계가 얼마나 변하는지(미분값)를 반환합니다.
    vec2 derivative = fwidth(scaledCoord);
    
    // 픽셀 단위 기준 선 중심으로부터의 거리 계산
    vec2 grid = abs(fract(scaledCoord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    
    // [해결 1] smoothstep을 이용한 부드러운 안티앨리어싱 (선명도 보장)
    // 두께(thickness)를 기준으로 1픽셀 내외로 부드럽게 페이드 아웃시킵니다.
    float alpha = 1.0 - smoothstep(thickness - 1.0, thickness + 1.0, line);
    
    // [해결 2] 카메라가 멀어질 때 촘촘해진 선이 뭉개지는 현상 방지 (LOD Fade)
    // derivative 값이 커진다는 것은 카메라가 멀어짐(Zoom Out)을 의미합니다.
    // 선이 너무 조밀해지기 전에 서서히 투명하게(Fade Out) 만듭니다.
    float lodFade = 1.0 - smoothstep(0.1, 0.8, length(derivative));
    
    // 최종 알파: 기본 투명도(0.5) * 선명도(alpha) * 카메라 거리에 따른 페이드(lodFade)
    return vec4(0.4, 0.4, 0.4, alpha * 0.5 * lodFade);
}

void main() {
    // 1. NDC 화면 좌표를 2D World 좌표(XY)로 역투영 (Unproject)
    mat4 invViewProj = inverse(commonFragdata.viewProjection);
    vec4 worldPos = invViewProj * vec4(inNDC, 0.0, 1.0);
    vec2 coord = worldPos.xy;

    // 2. 1m 단위의 얇은 선 (두께 1.0)
    vec4 grid1 = grid2D(coord, 1.0, 1.0);
    
    // 3. 10m 단위의 굵은 선 (두께 1.5)
    vec4 grid10 = grid2D(coord, 0.1, 1.5);
    
    // 4. 선 합성 (알파 블렌딩)
    // 더 굵은 선(10m)을 얇은 선 위에 덮어씁니다.
    vec4 color = mix(grid1, grid10, grid10.a);

    // 5. 중심 축(Axis) 색칠하기 (완벽한 픽셀 단위 두께로 수정)
    vec2 derivative = fwidth(coord);
    vec2 axisDist = abs(coord) / derivative; // 픽셀 단위 중심축으로부터의 거리
    
    float axisThickness = 1.5; // 축 두께 (픽셀)

    // smoothstep을 이용해 픽셀 단위로 축 선을 선명하게 그림
    float xAxisAlpha = 1.0 - smoothstep(axisThickness - 1.0, axisThickness + 1.0, axisDist.y); // X축 (Y=0)
    float yAxisAlpha = 1.0 - smoothstep(axisThickness - 1.0, axisThickness + 1.0, axisDist.x); // Y축 (X=0)

    // X축 (빨간색) 덧칠하기
    if (xAxisAlpha > 0.0) {
        color = mix(color, vec4(1.0, 0.3, 0.3, 1.0), xAxisAlpha);
    }
    // Y축 (초록색) 덧칠하기
    if (yAxisAlpha > 0.0) {
        color = mix(color, vec4(0.4, 1.0, 0.4, 1.0), yAxisAlpha);
    }

    outColor = color;
    
    // 최적화: 렌더링 부하 최소화를 위해 거의 안 보이는 픽셀 폐기
    if (outColor.a <= 0.02) discard;
}