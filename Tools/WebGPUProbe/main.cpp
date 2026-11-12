#include <cstdio>
#include <webgpu/webgpu.h>
#include <cmath>
#include <emscripten/emscripten.h>
namespace
{
    WGPUInstance instance = nullptr;
    WGPUSurface surface = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPURenderPipeline trianglePipeline = nullptr;
    WGPUQueue queue = nullptr;

    WGPURenderPipeline CreateTrianglePipeline(WGPUTextureFormat format)
    {
        constexpr char shaderCode[] = R"(
            @vertex fn vs(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
                let positions = array(vec2f(-0.65, -0.55), vec2f(0.65, -0.55), vec2f(0.0, 0.65));
                return vec4f(positions[index], 0.0, 1.0);
            }

            @fragment fn fs() -> @location(0) vec4f {
                return vec4f(1.0, 0.42, 0.10, 1.0);
            }
        )";

        WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
        source.code = { shaderCode, sizeof(shaderCode) - 1 };

        WGPUShaderModuleDescriptor shaderDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        shaderDesc.nextInChain = &source.chain;
        const auto shader = wgpuDeviceCreateShaderModule(device, &shaderDesc);

        WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
        target.format = format;

        WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
        fragment.module = shader;
        fragment.entryPoint = { "fs", 2 };
        fragment.targetCount = 1;
        fragment.targets = &target;

        WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pipelineDesc.vertex.module = shader;
        pipelineDesc.vertex.entryPoint = { "vs", 2 };
        pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pipelineDesc.fragment = &fragment;
        const auto pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
        wgpuShaderModuleRelease(shader);
        return pipeline;
    }

    void DrawTriangle(WGPUTexture texture, double timeMs)
    {
        const auto view = wgpuTextureCreateView(texture, nullptr);
        WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        color.view = view;
        color.loadOp = WGPULoadOp_Clear;
        color.storeOp = WGPUStoreOp_Store;
        const double blue = 0.24 + 0.16 * std::sin(timeMs * 0.002);
        color.clearValue = { 0.04, 0.07, blue, 1.0 };

        WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        passDesc.colorAttachmentCount = 1;
        passDesc.colorAttachments = &color;

        const auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
        const auto pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
        wgpuRenderPassEncoderSetPipeline(pass, trianglePipeline);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass);

        const auto commands = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(queue, 1, &commands);

        wgpuCommandBufferRelease(commands);
        wgpuRenderPassEncoderRelease(pass);
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(view);
    }

    void RenderFrame(void*)
    {
        WGPUSurfaceTexture frame = WGPU_SURFACE_TEXTURE_INIT;
        wgpuSurfaceGetCurrentTexture(surface, &frame);
        if (!frame.texture)
        {
            std::printf("WebGPU canvas texture unavailable (status=%d).\n", static_cast<int>(frame.status));
            emscripten_cancel_main_loop();
            return;
        }

        DrawTriangle(frame.texture, emscripten_get_now());
        wgpuTextureRelease(frame.texture);
    }

    void OnDeviceReady(WGPURequestDeviceStatus status, WGPUDevice result, WGPUStringView, void*, void*)
    {
        if (status != WGPURequestDeviceStatus_Success || !result)
        {
            std::puts("WebGPU device request failed.");
            return;
        }

        device = result;
        WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
        if (wgpuSurfaceGetCapabilities(surface, adapter, &capabilities) != WGPUStatus_Success)
        {
            std::puts("WebGPU canvas capabilities unavailable.");
            return;
        }

        if (capabilities.formatCount == 0)
        {
            wgpuSurfaceCapabilitiesFreeMembers(capabilities);
            std::puts("WebGPU canvas has no supported format.");
            return;
        }

        WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
        config.device = device;
        config.format = capabilities.formats[0];
        config.width = 640;
        config.height = 360;
        wgpuSurfaceConfigure(surface, &config);
        wgpuSurfaceCapabilitiesFreeMembers(capabilities);

        trianglePipeline = CreateTrianglePipeline(config.format);
        if (!trianglePipeline)
        {
            std::puts("WebGPU triangle pipeline creation failed.");
            return;
        }

        queue = wgpuDeviceGetQueue(device);
        emscripten_set_main_loop_arg(RenderFrame, nullptr, 0, false);
    }

    void OnAdapterReady(WGPURequestAdapterStatus status, WGPUAdapter result, WGPUStringView, void*, void*)
    {
        if (status != WGPURequestAdapterStatus_Success || !result)
        {
            std::puts("WebGPU adapter request failed.");
            return;
        }

        adapter = result;
        WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_AllowSpontaneous;
        callback.callback = OnDeviceReady;
        wgpuAdapterRequestDevice(adapter, nullptr, callback);
    }
}

int main()
{
    instance = wgpuCreateInstance(nullptr);
    if (!instance)
        return 1;

    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas = WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
    canvas.selector = { "#canvas", 7 };
    WGPUSurfaceDescriptor descriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
    descriptor.nextInChain = &canvas.chain;
    surface = wgpuInstanceCreateSurface(instance, &descriptor);
    if (!surface)
        return 1;

    WGPURequestAdapterCallbackInfo callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.callback = OnAdapterReady;
    wgpuInstanceRequestAdapter(instance, nullptr, callback);
    return 0;
}