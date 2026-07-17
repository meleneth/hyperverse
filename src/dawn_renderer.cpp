#include "hyperverse/dawn_renderer.hpp"

#include "png_rgba.hpp"

#include <SDL3/SDL.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#include <webgpu/webgpu_cpp.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hyperverse {
namespace {

struct LoadedSprite {
  std::uint32_t width{};
  std::uint32_t height{};
  std::vector<std::uint8_t> rgba{};
};

struct SpriteVertex {
  float x{};
  float y{};
  float u{};
  float v{};
  float r{};
  float g{};
  float b{};
  float a{};
};

struct LineVertex {
  float x{};
  float y{};
  float r{};
  float g{};
  float b{};
  float a{};
};

[[nodiscard]] LoadedSprite crop_rgba(
  const SpriteAlphaMask& source,
  const std::uint32_t x,
  const std::uint32_t y,
  const std::uint32_t width,
  const std::uint32_t height
) {
  LoadedSprite cropped{
    .width = width,
    .height = height,
    .rgba = std::vector<std::uint8_t>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U),
  };

  for (std::uint32_t row = 0; row < height; ++row) {
    const std::size_t source_offset = ((static_cast<std::size_t>(y + row) * source.width) + x) * 4U;
    const std::size_t destination_offset = static_cast<std::size_t>(row) * width * 4U;
    std::memcpy(cropped.rgba.data() + destination_offset, source.rgba.data() + source_offset, static_cast<std::size_t>(width) * 4U);
  }

  return cropped;
}

[[nodiscard]] LoadedSprite to_loaded_sprite(SpriteAlphaMask mask) {
  return LoadedSprite{.width = mask.width, .height = mask.height, .rgba = std::move(mask.rgba)};
}

[[nodiscard]] std::vector<LoadedSprite> load_sprite_textures() {
  std::vector<LoadedSprite> images;
  images.push_back(to_loaded_sprite(load_png_rgba("assets/sector7/sprites/ship.png")));
  images.push_back(to_loaded_sprite(load_png_rgba("assets/sector7/sprites/rock1.png")));
  images.push_back(to_loaded_sprite(load_png_rgba("assets/sector7/sprites/reticle.png")));
  images.push_back(to_loaded_sprite(load_png_rgba("assets/sector7/sprites/laser.png")));
  images.push_back(to_loaded_sprite(load_png_rgba("assets/sector7/sprites/robot.png")));
  images.push_back(to_loaded_sprite(load_png_rgba("assets/sector7/sprites/rocket.png")));
  images.push_back(to_loaded_sprite(load_png_rgba("assets/sector7/sprites/particle.png")));

  const SpriteAlphaMask alpha = load_png_rgba("assets/sector7/sprites/alpha.png");
  for (std::uint32_t index = 0; index < 26U; ++index) {
    images.push_back(crop_rgba(alpha, index * 8U, 0U, 8U, 16U));
  }

  const SpriteAlphaMask digits = load_png_rgba("assets/sector7/sprites/digits.png");
  for (std::uint32_t index = 0; index < 10U; ++index) {
    images.push_back(crop_rgba(digits, index * 8U, 0U, 8U, 16U));
  }

  return images;
}

[[nodiscard]] wgpu::ShaderModule create_shader_module(const wgpu::Device& device, const std::string_view source) {
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = wgpu::StringView{source};
  wgpu::ShaderModuleDescriptor descriptor{};
  descriptor.nextInChain = &wgsl;
  return device.CreateShaderModule(&descriptor);
}

[[nodiscard]] std::string to_string(const wgpu::StringView message) {
  const std::string_view view = message;
  return std::string{view};
}

[[nodiscard]] bool supports_present_mode(const wgpu::SurfaceCapabilities& capabilities, const wgpu::PresentMode mode) {
  for (std::size_t index = 0; index < capabilities.presentModeCount; ++index) {
    if (capabilities.presentModes[index] == mode) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::uint32_t aligned_bytes_per_row(const std::uint32_t width) {
  constexpr std::uint32_t alignment = 256U;
  const std::uint32_t bytes = width * 4U;
  return ((bytes + alignment - 1U) / alignment) * alignment;
}

void append_sprite_vertices(std::vector<SpriteVertex>& vertices, const SpriteDraw& sprite) {
  const float cosine = std::cos(sprite.rotation_radians);
  const float sine = std::sin(sprite.rotation_radians);
  const std::array<std::array<float, 4>, 6> corners{{
    {{-1.0F, -1.0F, 0.0F, 1.0F}},
    {{1.0F, -1.0F, 1.0F, 1.0F}},
    {{1.0F, 1.0F, 1.0F, 0.0F}},
    {{-1.0F, -1.0F, 0.0F, 1.0F}},
    {{1.0F, 1.0F, 1.0F, 0.0F}},
    {{-1.0F, 1.0F, 0.0F, 0.0F}},
  }};

  for (const auto& corner : corners) {
    const float local_x = corner[0] * sprite.half_width_ndc;
    const float local_y = corner[1] * sprite.half_height_ndc;
    vertices.push_back(
      SpriteVertex{
        .x = sprite.center_x_ndc + (local_x * cosine) - (local_y * sine),
        .y = sprite.center_y_ndc + (local_x * sine) + (local_y * cosine),
        .u = corner[2],
        .v = corner[3],
        .r = sprite.tint_r,
        .g = sprite.tint_g,
        .b = sprite.tint_b,
        .a = sprite.tint_a,
      }
    );
  }
}

}  // namespace

struct DawnRenderer::Impl {
  explicit Impl(SDL_Window& window) : window_{&window} {
    create_instance();
    create_surface();
    request_adapter();
    request_device();
    queue_ = device_.GetQueue();
    configure_surface();
    create_sampler();
    create_texture_bind_group_layout();
    create_pipelines();
    create_textures();
  }

  ~Impl() {
    wait_idle();
    if (surface_ != nullptr) {
      surface_.Unconfigure();
    }
  }

  void draw_frame(const SpriteFrame& frame) {
    instance_.ProcessEvents();
    refresh_extent();
    if (width_ == 0U || height_ == 0U) {
      return;
    }

    if (surface_dirty_) {
      configure_surface();
    }

    wgpu::SurfaceTexture surface_texture{};
    surface_.GetCurrentTexture(&surface_texture);
    if (surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
      surface_dirty_ = true;
      return;
    }

    wgpu::TextureView target_view = surface_texture.texture.CreateView();
    const RenderColor clear = make_clear_color(frame.state);

    wgpu::RenderPassColorAttachment color_attachment{};
    color_attachment.view = target_view;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    color_attachment.clearValue = wgpu::Color{
      .r = static_cast<double>(clear.r),
      .g = static_cast<double>(clear.g),
      .b = static_cast<double>(clear.b),
      .a = static_cast<double>(clear.a),
    };

    wgpu::RenderPassDescriptor pass_descriptor{};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &color_attachment;

    wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&pass_descriptor);
    draw_sprites(pass, frame.sprites);
    draw_lines(pass, frame.lines);
    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    queue_.Submit(1, &commands);
    if (surface_.Present() != wgpu::Status::Success) {
      surface_dirty_ = true;
    }
  }

  void wait_idle() const {
    if (device_ != nullptr) {
      device_.Tick();
    }
  }

  [[nodiscard]] std::uint32_t width() const {
    return width_;
  }

  [[nodiscard]] std::uint32_t height() const {
    return height_;
  }

  void create_instance() {
    wgpu::InstanceDescriptor descriptor{};
    instance_ = wgpu::CreateInstance(&descriptor);
    if (instance_ == nullptr) {
      throw std::runtime_error("failed to create Dawn instance");
    }
  }

  void create_surface() {
    SDL_PropertiesID properties = SDL_GetWindowProperties(window_);
    if (properties == 0U) {
      throw std::runtime_error(std::string{"SDL_GetWindowProperties failed: "} + SDL_GetError());
    }

    wgpu::SurfaceDescriptor descriptor{};
#if defined(__EMSCRIPTEN__)
    const char* canvas_id = SDL_GetStringProperty(properties, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING, nullptr);
    std::string selector = canvas_id == nullptr || std::string_view{canvas_id}.empty() ? std::string{"#canvas"} : std::string{canvas_id};
    if (!selector.empty() && selector.front() != '#' && selector.front() != '!') {
      selector.insert(selector.begin(), '#');
    }

    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvas_source{};
    canvas_source.selector = wgpu::StringView{selector};
    descriptor.nextInChain = &canvas_source;
#elif defined(_WIN32)
    void* hwnd = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    void* hinstance = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
    if (hwnd == nullptr || hinstance == nullptr) {
      throw std::runtime_error("SDL window does not expose Win32 handles for Dawn");
    }

    wgpu::SurfaceSourceWindowsHWND windows_source{};
    windows_source.hwnd = hwnd;
    windows_source.hinstance = hinstance;
    descriptor.nextInChain = &windows_source;
#else
#if defined(HYPERVERSE_ENABLE_X11)
    void* x11_display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    const Sint64 x11_window_id = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    wgpu::SurfaceSourceXlibWindow x11_source{};
#endif
    wgpu::SurfaceSourceWaylandSurface wayland_source{};
#if defined(HYPERVERSE_ENABLE_X11)
    if (x11_display != nullptr && x11_window_id > 0) {
      x11_source.display = x11_display;
      x11_source.window = static_cast<std::uint64_t>(x11_window_id);
      descriptor.nextInChain = &x11_source;
    } else {
#endif
      void* wayland_display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
      void* wayland_surface = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
      if (wayland_display == nullptr || wayland_surface == nullptr) {
#if defined(HYPERVERSE_ENABLE_X11)
        throw std::runtime_error("SDL window does not expose X11 or Wayland handles for Dawn");
#else
        throw std::runtime_error("SDL window does not expose Wayland handles for Dawn");
#endif
      }

      wayland_source.display = wayland_display;
      wayland_source.surface = wayland_surface;
      descriptor.nextInChain = &wayland_source;
#if defined(HYPERVERSE_ENABLE_X11)
    }
#endif
#endif

    surface_ = instance_.CreateSurface(&descriptor);
    if (surface_ == nullptr) {
      throw std::runtime_error("failed to create Dawn surface from SDL window");
    }
  }

  void request_adapter() {
    wgpu::RequestAdapterOptions options{};
    options.compatibleSurface = surface_;
    options.powerPreference = wgpu::PowerPreference::HighPerformance;

    wgpu::Future future = instance_.RequestAdapter(
      &options,
      wgpu::CallbackMode::WaitAnyOnly,
      [this](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
        if (status == wgpu::RequestAdapterStatus::Success) {
          adapter_ = adapter;
          return;
        }
        adapter_error_ = to_string(message);
      }
    );
    instance_.WaitAny(future, UINT64_MAX);
    if (adapter_ == nullptr) {
      throw std::runtime_error("failed to request Dawn adapter: " + adapter_error_);
    }
  }

  void request_device() {
    wgpu::DeviceDescriptor descriptor{};
    descriptor.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message) {
        const std::string text = to_string(message);
        SDL_Log("Dawn validation error: %s", text.c_str());
      }
    );

    wgpu::Future future = adapter_.RequestDevice(
      &descriptor,
      wgpu::CallbackMode::WaitAnyOnly,
      [this](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
        if (status == wgpu::RequestDeviceStatus::Success) {
          device_ = device;
          return;
        }
        device_error_ = to_string(message);
      }
    );
    instance_.WaitAny(future, UINT64_MAX);
    if (device_ == nullptr) {
      throw std::runtime_error("failed to request Dawn device: " + device_error_);
    }
  }

  void refresh_extent() {
    int drawable_width = 0;
    int drawable_height = 0;
    if (!SDL_GetWindowSizeInPixels(window_, &drawable_width, &drawable_height)) {
      return;
    }

    const std::uint32_t next_width = static_cast<std::uint32_t>(std::max(drawable_width, 0));
    const std::uint32_t next_height = static_cast<std::uint32_t>(std::max(drawable_height, 0));
    if (next_width != width_ || next_height != height_) {
      width_ = next_width;
      height_ = next_height;
      surface_dirty_ = true;
    }
  }

  void configure_surface() {
    refresh_extent();
    if (width_ == 0U || height_ == 0U) {
      return;
    }

    wgpu::SurfaceCapabilities capabilities{};
    surface_.GetCapabilities(adapter_, &capabilities);
    if (capabilities.formatCount == 0U || capabilities.presentModeCount == 0U || capabilities.alphaModeCount == 0U) {
      throw std::runtime_error("Dawn surface reported no usable capabilities");
    }

    surface_format_ = capabilities.formats[0];
    wgpu::SurfaceConfiguration config{};
    config.device = device_;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.format = surface_format_;
    config.width = width_;
    config.height = height_;
    config.presentMode = supports_present_mode(capabilities, wgpu::PresentMode::Fifo) ? wgpu::PresentMode::Fifo : capabilities.presentModes[0];
    config.alphaMode = capabilities.alphaModes[0];
    surface_.Configure(&config);
    surface_dirty_ = false;
  }

  void create_sampler() {
    wgpu::SamplerDescriptor descriptor{};
    descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
    descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
    descriptor.magFilter = wgpu::FilterMode::Nearest;
    descriptor.minFilter = wgpu::FilterMode::Nearest;
    sampler_ = device_.CreateSampler(&descriptor);
  }

  void create_texture_bind_group_layout() {
    std::array<wgpu::BindGroupLayoutEntry, 2> entries{};
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Fragment;
    entries[0].sampler.type = wgpu::SamplerBindingType::Filtering;
    entries[1].binding = 1;
    entries[1].visibility = wgpu::ShaderStage::Fragment;
    entries[1].texture.sampleType = wgpu::TextureSampleType::Float;
    entries[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    wgpu::BindGroupLayoutDescriptor descriptor{};
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    texture_bind_group_layout_ = device_.CreateBindGroupLayout(&descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor{};
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &texture_bind_group_layout_;
    sprite_pipeline_layout_ = device_.CreatePipelineLayout(&pipeline_layout_descriptor);
  }

  void create_pipelines() {
    create_sprite_pipeline();
    create_line_pipeline();
  }

  void create_sprite_pipeline() {
    constexpr std::string_view shader = R"(
struct SpriteInput {
  @location(0) position: vec2f,
  @location(1) uv: vec2f,
  @location(2) tint: vec4f,
};

struct SpriteOutput {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
  @location(1) tint: vec4f,
};

@vertex
fn vs_main(input: SpriteInput) -> SpriteOutput {
  var output: SpriteOutput;
  output.position = vec4f(input.position, 0.0, 1.0);
  output.uv = input.uv;
  output.tint = input.tint;
  return output;
}

@group(0) @binding(0) var sprite_sampler: sampler;
@group(0) @binding(1) var sprite_texture: texture_2d<f32>;

@fragment
fn fs_main(input: SpriteOutput) -> @location(0) vec4f {
  return textureSample(sprite_texture, sprite_sampler, input.uv) * input.tint;
}
)";

    wgpu::ShaderModule module = create_shader_module(device_, shader);
    std::array<wgpu::VertexAttribute, 3> attributes{};
    attributes[0].format = wgpu::VertexFormat::Float32x2;
    attributes[0].offset = offsetof(SpriteVertex, x);
    attributes[0].shaderLocation = 0;
    attributes[1].format = wgpu::VertexFormat::Float32x2;
    attributes[1].offset = offsetof(SpriteVertex, u);
    attributes[1].shaderLocation = 1;
    attributes[2].format = wgpu::VertexFormat::Float32x4;
    attributes[2].offset = offsetof(SpriteVertex, r);
    attributes[2].shaderLocation = 2;

    wgpu::VertexBufferLayout vertex_buffer{};
    vertex_buffer.arrayStride = sizeof(SpriteVertex);
    vertex_buffer.attributeCount = attributes.size();
    vertex_buffer.attributes = attributes.data();

    wgpu::BlendState blend{};
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::One;
    blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState color_target{};
    color_target.format = surface_format_;
    color_target.blend = &blend;
    color_target.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    wgpu::RenderPipelineDescriptor descriptor{};
    descriptor.layout = sprite_pipeline_layout_;
    descriptor.vertex.module = module;
    descriptor.vertex.entryPoint = "vs_main";
    descriptor.vertex.bufferCount = 1;
    descriptor.vertex.buffers = &vertex_buffer;
    descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    descriptor.primitive.cullMode = wgpu::CullMode::None;
    descriptor.multisample.count = 1;
    descriptor.fragment = &fragment;
    sprite_pipeline_ = device_.CreateRenderPipeline(&descriptor);
  }

  void create_line_pipeline() {
    constexpr std::string_view shader = R"(
struct LineInput {
  @location(0) position: vec2f,
  @location(1) color: vec4f,
};

struct LineOutput {
  @builtin(position) position: vec4f,
  @location(0) color: vec4f,
};

@vertex
fn vs_main(input: LineInput) -> LineOutput {
  var output: LineOutput;
  output.position = vec4f(input.position, 0.0, 1.0);
  output.color = input.color;
  return output;
}

@fragment
fn fs_main(input: LineOutput) -> @location(0) vec4f {
  return input.color;
}
)";

    wgpu::ShaderModule module = create_shader_module(device_, shader);
    std::array<wgpu::VertexAttribute, 2> attributes{};
    attributes[0].format = wgpu::VertexFormat::Float32x2;
    attributes[0].offset = offsetof(LineVertex, x);
    attributes[0].shaderLocation = 0;
    attributes[1].format = wgpu::VertexFormat::Float32x4;
    attributes[1].offset = offsetof(LineVertex, r);
    attributes[1].shaderLocation = 1;

    wgpu::VertexBufferLayout vertex_buffer{};
    vertex_buffer.arrayStride = sizeof(LineVertex);
    vertex_buffer.attributeCount = attributes.size();
    vertex_buffer.attributes = attributes.data();

    wgpu::BlendState blend{};
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::One;
    blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState color_target{};
    color_target.format = surface_format_;
    color_target.blend = &blend;
    color_target.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    wgpu::RenderPipelineDescriptor descriptor{};
    descriptor.vertex.module = module;
    descriptor.vertex.entryPoint = "vs_main";
    descriptor.vertex.bufferCount = 1;
    descriptor.vertex.buffers = &vertex_buffer;
    descriptor.primitive.topology = wgpu::PrimitiveTopology::LineList;
    descriptor.primitive.cullMode = wgpu::CullMode::None;
    descriptor.multisample.count = 1;
    descriptor.fragment = &fragment;
    line_pipeline_ = device_.CreateRenderPipeline(&descriptor);
  }

  void create_textures() {
    for (const LoadedSprite& sprite : load_sprite_textures()) {
      wgpu::TextureDescriptor texture_descriptor{};
      texture_descriptor.size = wgpu::Extent3D{.width = sprite.width, .height = sprite.height, .depthOrArrayLayers = 1};
      texture_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
      texture_descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
      texture_descriptor.dimension = wgpu::TextureDimension::e2D;
      wgpu::Texture texture = device_.CreateTexture(&texture_descriptor);

      const std::uint32_t bytes_per_row = aligned_bytes_per_row(sprite.width);
      std::vector<std::uint8_t> upload(static_cast<std::size_t>(bytes_per_row) * sprite.height);
      for (std::uint32_t row = 0; row < sprite.height; ++row) {
        std::memcpy(
          upload.data() + (static_cast<std::size_t>(row) * bytes_per_row),
          sprite.rgba.data() + (static_cast<std::size_t>(row) * sprite.width * 4U),
          static_cast<std::size_t>(sprite.width) * 4U
        );
      }

      wgpu::TexelCopyTextureInfo destination{};
      destination.texture = texture;
      wgpu::TexelCopyBufferLayout source_layout{};
      source_layout.bytesPerRow = bytes_per_row;
      source_layout.rowsPerImage = sprite.height;
      queue_.WriteTexture(&destination, upload.data(), upload.size(), &source_layout, &texture_descriptor.size);

      wgpu::TextureView view = texture.CreateView();
      std::array<wgpu::BindGroupEntry, 2> entries{};
      entries[0].binding = 0;
      entries[0].sampler = sampler_;
      entries[1].binding = 1;
      entries[1].textureView = view;

      wgpu::BindGroupDescriptor bind_group_descriptor{};
      bind_group_descriptor.layout = texture_bind_group_layout_;
      bind_group_descriptor.entryCount = entries.size();
      bind_group_descriptor.entries = entries.data();
      texture_bind_groups_.push_back(device_.CreateBindGroup(&bind_group_descriptor));
      textures_.push_back(texture);
      texture_views_.push_back(view);
    }
  }

  void draw_sprites(wgpu::RenderPassEncoder& pass, const std::vector<SpriteDraw>& sprites) {
    if (sprites.empty()) {
      return;
    }

    pass.SetPipeline(sprite_pipeline_);
    for (const SpriteDraw& sprite : sprites) {
      const std::size_t texture_index = static_cast<std::size_t>(sprite.texture);
      if (texture_index >= texture_bind_groups_.size()) {
        continue;
      }

      std::vector<SpriteVertex> vertices;
      vertices.reserve(6);
      append_sprite_vertices(vertices, sprite);
      const wgpu::Buffer vertex_buffer = transient_buffer(vertices.data(), sizeof(SpriteVertex) * vertices.size());
      pass.SetBindGroup(0, texture_bind_groups_[texture_index]);
      pass.SetVertexBuffer(0, vertex_buffer);
      pass.Draw(static_cast<std::uint32_t>(vertices.size()));
    }
  }

  void draw_lines(wgpu::RenderPassEncoder& pass, const std::vector<LineDraw>& lines) {
    if (lines.empty()) {
      return;
    }

    std::vector<LineVertex> vertices;
    vertices.reserve(lines.size() * 2U);
    for (const LineDraw& line : lines) {
      vertices.push_back(LineVertex{.x = line.start_x_ndc, .y = line.start_y_ndc, .r = line.r, .g = line.g, .b = line.b, .a = line.a});
      vertices.push_back(LineVertex{.x = line.end_x_ndc, .y = line.end_y_ndc, .r = line.r, .g = line.g, .b = line.b, .a = line.a});
    }

    const wgpu::Buffer vertex_buffer = transient_buffer(vertices.data(), sizeof(LineVertex) * vertices.size());
    pass.SetPipeline(line_pipeline_);
    pass.SetVertexBuffer(0, vertex_buffer);
    pass.Draw(static_cast<std::uint32_t>(vertices.size()));
  }

  [[nodiscard]] wgpu::Buffer transient_buffer(const void* data, const std::size_t size) {
    wgpu::BufferDescriptor descriptor{};
    descriptor.size = size;
    descriptor.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device_.CreateBuffer(&descriptor);
    queue_.WriteBuffer(buffer, 0, data, size);
    return buffer;
  }

  SDL_Window* window_{nullptr};
  wgpu::Instance instance_{};
  wgpu::Surface surface_{};
  wgpu::Adapter adapter_{};
  wgpu::Device device_{};
  wgpu::Queue queue_{};
  wgpu::TextureFormat surface_format_{wgpu::TextureFormat::Undefined};
  std::uint32_t width_{1280U};
  std::uint32_t height_{720U};
  bool surface_dirty_{true};
  std::string adapter_error_{};
  std::string device_error_{};

  wgpu::Sampler sampler_{};
  wgpu::BindGroupLayout texture_bind_group_layout_{};
  wgpu::PipelineLayout sprite_pipeline_layout_{};
  wgpu::RenderPipeline sprite_pipeline_{};
  wgpu::RenderPipeline line_pipeline_{};
  std::vector<wgpu::Texture> textures_{};
  std::vector<wgpu::TextureView> texture_views_{};
  std::vector<wgpu::BindGroup> texture_bind_groups_{};
};

DawnRenderer::DawnRenderer(SDL_Window& window) : impl_{std::make_unique<Impl>(window)} {}

DawnRenderer::~DawnRenderer() = default;

void DawnRenderer::draw_frame(const FrameSnapshot& frame) {
  draw_frame(SpriteFrame{.state = frame});
}

void DawnRenderer::draw_frame(const SpriteFrame& frame) {
  impl_->draw_frame(frame);
}

void DawnRenderer::wait_idle() const {
  impl_->wait_idle();
}

std::uint32_t DawnRenderer::width() const {
  return impl_->width();
}

std::uint32_t DawnRenderer::height() const {
  return impl_->height();
}

}  // namespace hyperverse
