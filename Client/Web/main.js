import { multiply, projectionMatrix, readMovementBindings, readScene, viewMatrix } from "./scene.js";

const canvas = document.querySelector("#canvas");
const status = document.querySelector("#status");
const controls = document.querySelector("#controls");

function reportError(error) {
    status.dataset.error = "true";
    status.textContent = error instanceof Error ? error.message : String(error);
    console.error(error);
}

async function fetchFile(name, format) {
    const response = await fetch(new URL(name, import.meta.url));
    if (!response.ok) throw new Error(`${name} 로드 실패 (${response.status})`);
    return response[format]();
}

function cubeVertices() {
    const vertices = [];
    for (let axis = 0; axis < 3; ++axis) {
        for (const side of [-1, 1]) {
            for (const [u, v] of [[-1, -1], [1, -1], [1, 1], [-1, -1], [1, 1], [-1, 1]]) {
                const position = [0, 0, 0];
                position[axis] = side * 0.5;
                position[(axis + 1) % 3] = u * 0.5;
                position[(axis + 2) % 3] = v * 0.5;
                vertices.push(...position, [0.75, 0.95, 0.62][axis] + side * 0.05);
            }
        }
    }
    return vertices;
}

function sphereVertices() {
    const vertices = [];
    const rings = 16;
    const sectors = 24;
    function addVertex(ring, sector) {
        const latitude = Math.PI * ring / rings;
        const longitude = 2 * Math.PI * sector / sectors;
        const x = Math.sin(latitude) * Math.cos(longitude);
        const y = Math.cos(latitude);
        const z = Math.sin(latitude) * Math.sin(longitude);
        vertices.push(x * 0.5, y * 0.5, z * 0.5, 0.72 + 0.21 * y - 0.06 * z);
    }
    for (let ring = 0; ring < rings; ++ring) {
        for (let sector = 0; sector < sectors; ++sector) {
            addVertex(ring, sector);
            addVertex(ring + 1, sector);
            addVertex(ring + 1, sector + 1);
            addVertex(ring, sector);
            addVertex(ring + 1, sector + 1);
            addVertex(ring, sector + 1);
        }
    }
    return vertices;
}

function uploadVertexBuffer(device, data) {
    const buffer = device.createBuffer({ size: data.byteLength, usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST });
    device.queue.writeBuffer(buffer, 0, data);
    return buffer;
}

function createDraws(device, meshes) {
    const draws = [];
    for (const [key, createVertices, color] of [
        ["CubeMesh", cubeVertices, [0.47, 0.73, 0.9]],
        ["SphereMesh", sphereVertices, [0.97, 0.64, 0.35]],
    ]) {
        const instances = meshes.filter(({ mesh }) => mesh === key);
        if (!instances.length) continue;
        const instanceData = new Float32Array(instances.length * 19);
        instances.forEach(({ world }, index) => {
            instanceData.set(world, index * 19);
            instanceData.set(color, index * 19 + 16);
        });
        const vertices = new Float32Array(createVertices());
        draws.push({
            vertices: uploadVertexBuffer(device, vertices),
            instances: uploadVertexBuffer(device, instanceData),
            vertexCount: vertices.length / 4,
            instanceCount: instances.length,
        });
    }
    return draws;
}

function bindControls(bindings) {
    const boundKeys = new Set([...bindings.values()].flatMap((keys) => [...keys]));
    const pressed = new Set();
    const clearKeys = () => pressed.clear();
    canvas.addEventListener("pointerdown", () => canvas.focus());
    canvas.addEventListener("keydown", (event) => {
        if (!boundKeys.has(event.code) || event.ctrlKey || event.metaKey || event.altKey) return;
        event.preventDefault();
        pressed.add(event.code);
    });
    window.addEventListener("keyup", (event) => pressed.delete(event.code));
    canvas.addEventListener("blur", clearKeys);
    window.addEventListener("blur", clearKeys);
    document.addEventListener("visibilitychange", clearKeys);

    const keys = [...boundKeys].map((key) => key.slice(3)).sort();
    controls.textContent = keys.length
        ? `화면을 클릭한 뒤 ${keys.join(" / ")} 키로 카메라를 이동하세요.`
        : "Gameplay에 카메라 이동에 사용할 영문 키 바인딩이 없습니다.";
    controls.hidden = false;

    return (action) => Number([...bindings.get(action)].some((key) => pressed.has(key)));
}

async function start() {
    if (!navigator.gpu) throw new Error("WebGPU를 사용할 수 없습니다. 지원 브라우저에서 HTTPS 또는 localhost로 열어 주세요.");
    const [sceneArchive, inputActions, shaderSource, adapter] = await Promise.all([
        fetchFile("StartupScene.bamscene.json", "json"),
        fetchFile("InputActions.json", "json"),
        fetchFile("scene.wgsl", "text"),
        navigator.gpu.requestAdapter(),
    ]);
    if (!adapter) throw new Error("WebGPU 어댑터를 찾을 수 없습니다.");
    const scene = readScene(sceneArchive);
    const device = await adapter.requestDevice();
    const context = canvas.getContext("webgpu");
    if (!context) throw new Error("WebGPU 캔버스를 초기화할 수 없습니다.");
    const format = navigator.gpu.getPreferredCanvasFormat();
    context.configure({ device, format, alphaMode: "opaque" });

    const shader = device.createShaderModule({ code: shaderSource });
    const pipeline = await device.createRenderPipelineAsync({
        layout: "auto",
        vertex: {
            module: shader, entryPoint: "vs",
            buffers: [
                { arrayStride: 16, attributes: [
                    { shaderLocation: 0, offset: 0, format: "float32x3" },
                    { shaderLocation: 1, offset: 12, format: "float32" },
                ] },
                { arrayStride: 76, stepMode: "instance", attributes: [
                    ...[0, 1, 2, 3].map((column) => ({ shaderLocation: 2 + column, offset: 16 * column, format: "float32x4" })),
                    { shaderLocation: 6, offset: 64, format: "float32x3" },
                ] },
            ],
        },
        fragment: { module: shader, entryPoint: "fs", targets: [{ format }] },
        primitive: { topology: "triangle-list", cullMode: "none" },
        depthStencil: { format: "depth24plus", depthWriteEnabled: true, depthCompare: "less" },
    });

    const cameraBuffer = device.createBuffer({ size: 64, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
    const cameraGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [{ binding: 0, resource: { buffer: cameraBuffer } }],
    });
    const draws = createDraws(device, scene.meshes);
    const action = bindControls(readMovementBindings(inputActions));
    const camera = scene.camera.world;
    const right = camera.subarray(0, 3);
    const forward = camera.subarray(8, 11);
    const rightLength = Math.hypot(...right);
    const forwardLength = Math.hypot(...forward);
    viewMatrix(camera);

    let depthTexture;
    let previousTime;
    let frameId;
    function stop(error) {
        if (frameId === undefined) return;
        cancelAnimationFrame(frameId);
        frameId = undefined;
        reportError(error);
    }
    device.lost.then(({ message }) => stop(`WebGPU 장치 연결이 끊겼습니다. ${message}`));
    device.addEventListener("uncapturederror", ({ error }) => stop(error));

    function frame(time) {
        try {
            const dt = previousTime === undefined ? 0 : Math.min((time - previousTime) / 1000, 0.05);
            previousTime = time;
            const x = action("MoveRight") - action("MoveLeft");
            const z = action("MoveForward") - action("MoveBackward");
            const distance = 7 * dt / Math.max(1, Math.hypot(x, z));
            for (let axis = 0; axis < 3; ++axis) {
                camera[12 + axis] += distance * (right[axis] * x / rightLength + forward[axis] * z / forwardLength);
            }

            const pixelRatio = window.devicePixelRatio || 1;
            const limit = device.limits.maxTextureDimension2D;
            const width = Math.max(1, Math.min(limit, Math.round(canvas.clientWidth * pixelRatio)));
            const height = Math.max(1, Math.min(limit, Math.round(canvas.clientHeight * pixelRatio)));
            if (!depthTexture || canvas.width !== width || canvas.height !== height) {
                canvas.width = width;
                canvas.height = height;
                depthTexture?.destroy();
                depthTexture = device.createTexture({ size: [width, height], format: "depth24plus", usage: GPUTextureUsage.RENDER_ATTACHMENT });
            }

            const viewProjection = multiply(projectionMatrix(scene.camera.settings, width / height), viewMatrix(camera));
            device.queue.writeBuffer(cameraBuffer, 0, viewProjection);
            const encoder = device.createCommandEncoder();
            const pass = encoder.beginRenderPass({
                colorAttachments: [{
                    view: context.getCurrentTexture().createView(),
                    clearValue: { r: 0.05, g: 0.08, b: 0.12, a: 1 }, loadOp: "clear", storeOp: "store",
                }],
                depthStencilAttachment: {
                    view: depthTexture.createView(), depthClearValue: 1, depthLoadOp: "clear", depthStoreOp: "discard",
                },
            });
            pass.setPipeline(pipeline);
            pass.setBindGroup(0, cameraGroup);
            for (const draw of draws) {
                pass.setVertexBuffer(0, draw.vertices);
                pass.setVertexBuffer(1, draw.instances);
                pass.draw(draw.vertexCount, draw.instanceCount);
            }
            pass.end();
            device.queue.submit([encoder.finish()]);
            frameId = requestAnimationFrame(frame);
        } catch (error) {
            stop(error);
        }
    }

    const skipped = scene.skippedMeshes ? ` · 생략한 정적 메시 ${scene.skippedMeshes}개` : "";
    status.textContent = `${scene.name} · 기본 도형 ${scene.meshes.length}개${skipped}`;
    frameId = requestAnimationFrame(frame);
}

start().catch(reportError);
