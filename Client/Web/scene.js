// BamEngine scene archive v1 stores transforms as position/scale XYZ and rotation XYZW.
const active = 1;
const visible = 2;
const dead = 8;
const supportedMeshes = new Set(["CubeMesh", "SphereMesh"]);

const identity = new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);

export function multiply(a, b) {
    const result = new Float32Array(16);
    for (let column = 0; column < 4; ++column) {
        for (let row = 0; row < 4; ++row) {
            result[column * 4 + row] = a[row] * b[column * 4] + a[4 + row] * b[column * 4 + 1]
                + a[8 + row] * b[column * 4 + 2] + a[12 + row] * b[column * 4 + 3];
        }
    }
    return result;
}

function vector(value, size, field) {
    if (!Array.isArray(value) || value.length !== size || !value.every(Number.isFinite)) {
        throw new Error(`잘못된 씬 필드 ${field}`);
    }
    return value;
}

function localMatrix(transform, field) {
    if (!transform) return identity;
    const [px, py, pz] = vector(transform.position, 3, `${field}.position`);
    const [sx, sy, sz] = vector(transform.scale, 3, `${field}.scale`);
    const rotation = vector(transform.rotation, 4, `${field}.rotation`);
    const length = Math.hypot(...rotation);
    if (length === 0) throw new Error(`잘못된 씬 필드 ${field}.rotation`);
    const [x, y, z, w] = rotation.map((value) => value / length);
    return new Float32Array([
        (1 - 2 * (y * y + z * z)) * sx, 2 * (x * y + z * w) * sx, 2 * (x * z - y * w) * sx, 0,
        2 * (x * y - z * w) * sy, (1 - 2 * (x * x + z * z)) * sy, 2 * (y * z + x * w) * sy, 0,
        2 * (x * z + y * w) * sz, 2 * (y * z - x * w) * sz, (1 - 2 * (x * x + y * y)) * sz, 0,
        px, py, pz, 1,
    ]);
}

function cross(a, b) {
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
}

function dot(a, b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

export function viewMatrix(world) {
    const [a, b, c, position] = [0, 4, 8, 12].map((offset) => world.subarray(offset, offset + 3));
    const rowX = cross(b, c);
    const rowY = cross(c, a);
    const rowZ = cross(a, b);
    const det = dot(a, rowX);
    if (!Number.isFinite(det) || det === 0) throw new Error("카메라 변환을 역산할 수 없습니다.");
    return new Float32Array([
        rowX[0] / det, rowY[0] / det, rowZ[0] / det, 0,
        rowX[1] / det, rowY[1] / det, rowZ[1] / det, 0,
        rowX[2] / det, rowY[2] / det, rowZ[2] / det, 0,
        -dot(rowX, position) / det, -dot(rowY, position) / det, -dot(rowZ, position) / det, 1,
    ]);
}

// Match CameraSystem's left-handed projection and zero-to-one depth range.
export function projectionMatrix(camera, aspect) {
    const near = camera.NearPlane;
    const far = camera.FarPlane;
    if (!Number.isFinite(near) || !Number.isFinite(far) || far <= near) {
        throw new Error("카메라의 NearPlane/FarPlane 값이 잘못됐습니다.");
    }
    if (camera.CameraType === 0 && near > 0 && camera.FOV > 0 && camera.FOV < 180) {
        const y = 1 / Math.tan(camera.FOV * Math.PI / 360);
        const z = far / (far - near);
        return new Float32Array([y / aspect, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 1, 0, 0, -near * z, 0]);
    }
    if (camera.CameraType === 1 && Number.isFinite(camera.OrthographicSize) && camera.OrthographicSize > 0) {
        const y = 2 / camera.OrthographicSize;
        const z = 1 / (far - near);
        return new Float32Array([y / aspect, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, -near * z, 1]);
    }
    throw new Error("카메라 투영 설정을 읽을 수 없습니다.");
}

export function readScene(document) {
    const scene = document?.Scene;
    if (scene?.Version !== 1 || !Array.isArray(scene.Entities) || scene.Active === false) {
        throw new Error("활성 상태인 BamEngine 씬 v1 파일이 필요합니다.");
    }

    const entities = new Map();
    for (const entity of scene.Entities) {
        if (!Number.isSafeInteger(entity.Id) || entity.Id <= 0 || entities.has(entity.Id) || !Array.isArray(entity.Components)) {
            throw new Error(`잘못된 씬 엔티티 ${entity.Id}`);
        }
        const components = new Map(entity.Components.map(({ Type, Data }) => [Type, Data]));
        entities.set(entity.Id, {
            parent: entity.Hierarchy?.Parent ?? 0,
            flags: components.get("Engine::FlagComponent")?.flags ?? (active | visible),
            transform: components.get("Engine::TransformComponent"),
            camera: components.get("Engine::CameraComponent"),
            renderer: components.get("Engine::StaticMeshRendererComponent"),
        });
    }

    const resolved = new Map();
    const visiting = new Set();
    function resolve(id) {
        if (resolved.has(id)) return resolved.get(id);
        if (visiting.has(id) || !entities.has(id)) throw new Error(`잘못된 씬 계층 ${id}`);
        visiting.add(id);
        const entity = entities.get(id);
        const parent = entity.parent ? resolve(entity.parent) : null;
        const absolute = entity.transform?.absolute ?? 0;
        if (parent && absolute !== 0 && absolute !== 7) {
            throw new Error(`엔티티 ${id}의 혼합 절대 변환은 WebGPU Client에서 아직 지원하지 않습니다.`);
        }
        const local = localMatrix(entity.transform, `Entities[${id}].TransformComponent`);
        const state = {
            world: parent && absolute === 0 ? multiply(parent.world, local) : local,
            active: (entity.flags & (active | dead)) === active && (!parent || parent.active),
            visible: Boolean(entity.flags & visible) && (!parent || parent.visible),
        };
        visiting.delete(id);
        resolved.set(id, state);
        return state;
    }

    let camera;
    let skippedMeshes = 0;
    const meshes = [];
    for (const [id, entity] of entities) {
        if (!entity.camera && !entity.renderer) continue;
        const state = resolve(id);
        if (!state.active) continue;
        if (entity.camera && entity.transform && (!camera || (entity.camera.IsMainCamera && !camera.settings.IsMainCamera))) {
            camera = { settings: entity.camera, world: new Float32Array(state.world) };
        }
        if (!entity.renderer || !entity.transform || !state.visible) continue;
        const { meshHandle, materialHandle } = entity.renderer;
        const mesh = meshHandle?.AssetKey || meshHandle?.AssetPath;
        const material = materialHandle?.AssetKey || materialHandle?.AssetPath || "";
        if (meshHandle?.Null || !supportedMeshes.has(mesh) || /(?:^|[/\\])SkyMaterial\.bammat\.json$/i.test(material)) {
            ++skippedMeshes;
            continue;
        }
        meshes.push({ mesh, world: state.world });
    }

    if (!camera) throw new Error("시작 씬에 활성 카메라가 없습니다.");
    if (!meshes.length) throw new Error("시작 씬에 표시할 활성 CubeMesh 또는 SphereMesh가 없습니다.");
    return { name: scene.Name || "시작 씬", camera, meshes, skippedMeshes };
}

export function readMovementBindings(document) {
    const gameplay = document?.Settings?.Maps?.find(({ Id }) => Id === "Gameplay");
    const movement = new Map([
        ["MoveLeft", new Set()], ["MoveRight", new Set()],
        ["MoveForward", new Set()], ["MoveBackward", new Set()],
    ]);
    for (const { ActionId, Key } of gameplay?.Bindings ?? []) {
        if (movement.has(ActionId) && typeof Key === "string" && /^[a-z]$/i.test(Key)) {
            movement.get(ActionId).add(`Key${Key.toUpperCase()}`);
        }
    }
    return movement;
}
