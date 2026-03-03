#version 450
#include "common.glsl"

// 버텍스 셰이더(infinite_grid.vert)의 출력과 완벽히 일치합니다.
layout(location = 0) in vec2 inNDC;

layout(location = 0) out vec4 outColor;

// NDC 좌표를 3D 월드 좌표계로 역투영하는 함수
vec3 UnprojectPoint(vec3 ndc) {
    mat4 invViewProj = inverse(commonFragdata.viewProjection);
    vec4 worldPos = invViewProj * vec4(ndc, 1.0);
    return worldPos.xyz / worldPos.w;
}

// 3D 그리드 그리기 헬퍼 함수
vec4 grid3D(vec3 fragPos3D, float scale, float thickness, bool drawAxis) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    
    // 선 굵기 조절
    float line = min(grid.x, grid.y) / thickness;
    
    float minimumz = min(derivative.y, 1.0);
    float minimumx = min(derivative.x, 1.0);
    
    // 기본 선 색상 (회색)
    vec4 color = vec4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));

    // 중심 축(Axis) 하이라이트
    if(drawAxis) {
        // Z축 (파란색)
        if(fragPos3D.x > -0.1 * minimumx * thickness && fragPos3D.x < 0.1 * minimumx * thickness)
            color.rgb = vec3(0.2, 0.2, 1.0);
        // X축 (빨간색)
        if(fragPos3D.z > -0.1 * minimumz * thickness && fragPos3D.z < 0.1 * minimumz * thickness)
            color.rgb = vec3(1.0, 0.2, 0.2);
    }
    return color;
}

void main()
{
    // 1. 카메라 렌즈의 Near 평면과 Far 평면에서의 3D 좌표 도출
    vec3 nearPoint = UnprojectPoint(vec3(inNDC, 0.0));
    vec3 farPoint  = UnprojectPoint(vec3(inNDC, 1.0));

    // 2. 광선(Ray)과 바닥 평면(Y=0)의 교차점 수학적 계산
    vec3 rayDir = farPoint - nearPoint;
    float t = -nearPoint.y / rayDir.y;

    // 카메라가 하늘을 보고 있거나 교차점이 등 뒤에 있으면 그리지 않음
    if (t < 0.0) discard;

    // 3. 바닥(그리드)의 정확한 3D 월드 좌표 계산
    vec3 fragPos3D = nearPoint + t * rayDir;

    // 4. 깊이(Depth) 계산 및 덮어쓰기
    // 씬의 다른 3D 객체(사물)와 정상적으로 가려지기 위해 Z버퍼 값을 계산해 넣습니다.
    vec4 clip_space_pos = commonFragdata.viewProjection * vec4(fragPos3D, 1.0);
    gl_FragDepth = clip_space_pos.z / clip_space_pos.w;

    // 5. 단위별 굵은 선 합성 (1m, 10m, 100m)
    vec4 grid1   = grid3D(fragPos3D, 1.0, 1.0, false);   // 1m 단위 (얇은 선)
    vec4 grid10  = grid3D(fragPos3D, 0.1, 2.5, true);    // 10m 단위 (중간 굵은 선 + 축 표시)
    vec4 grid100 = grid3D(fragPos3D, 0.01, 5.0, true);   // 100m 단위 (아주 굵은 선 + 축 표시)

    // 밝기와 투명도를 합성합니다.
    vec4 color = grid1 + grid10 + grid100;
    color.a = max(grid1.a, max(grid10.a, grid100.a));

    // 6. 페이드 아웃 (거리에 따라 서서히 투명해짐)
    // commondata에 있는 카메라 위치(cameraPos)를 활용하여 자연스럽게 사라지도록 합니다.
    float distanceToCamera = length(fragPos3D - commonFragdata.cameraPos);
    float fading = max(0.0, 1.0 - (distanceToCamera / 150.0)); // 150 유닛 밖은 투명해짐

    color.a *= fading;

    // 불필요한 픽셀은 렌더링 단계에서 드랍하여 최적화
    if (color.a <= 0.0) discard;

    outColor = color;
}