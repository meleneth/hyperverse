#include "hyperverse/vulkan_renderer.hpp"

#include "hyperverse/model_mesh.hpp"

#include "png_rgba.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(__GNUC__)
#pragma GCC diagnostic push
// Vulkan structures are zero-initialized before designated fields are assigned.
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

namespace hyperverse {
namespace {

constexpr std::uint32_t invalid_queue = std::numeric_limits<std::uint32_t>::max();

void vk_check(const VkResult result, const char* operation) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::string{operation} + " failed with VkResult " + std::to_string(result));
  }
}

struct LoadedSprite {
  std::uint32_t width{};
  std::uint32_t height{};
  std::vector<std::uint8_t> rgba{};
};

struct SpriteVertex { float x{}, y{}, u{}, v{}, r{}, g{}, b{}, a{}; };
struct ColorVertex { float x{}, y{}, r{}, g{}, b{}, a{}; };
struct TriangleVertex { float x{}, y{}, u{}, v{}, r{}, g{}, b{}, a{}; };
struct TrailVertex { float x{}, y{}, normalized_age{}, intensity{}, side{}; };

[[nodiscard]] LoadedSprite crop_rgba(
  const SpriteAlphaMask& source,
  const std::uint32_t x,
  const std::uint32_t y,
  const std::uint32_t width,
  const std::uint32_t height
) {
  LoadedSprite result{
    .width = width,
    .height = height,
    .rgba = std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4U),
  };
  for (std::uint32_t row = 0; row < height; ++row) {
    const std::size_t source_offset = ((static_cast<std::size_t>(y + row) * source.width) + x) * 4U;
    const std::size_t destination_offset = static_cast<std::size_t>(row) * width * 4U;
    std::memcpy(result.rgba.data() + destination_offset, source.rgba.data() + source_offset, static_cast<std::size_t>(width) * 4U);
  }
  return result;
}

[[nodiscard]] LoadedSprite to_loaded_sprite(SpriteAlphaMask mask) {
  return {.width = mask.width, .height = mask.height, .rgba = std::move(mask.rgba)};
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
  images.push_back(to_loaded_sprite(load_png_rgba("assets/models/drone/textures/drone-friendly-albedo.png")));
  images.push_back(to_loaded_sprite(load_png_rgba("assets/models/drone/textures/drone-hostile-albedo.png")));
  return images;
}

void append_sprite_vertices(
  std::vector<SpriteVertex>& vertices,
  const SpriteDraw& sprite,
  const std::uint32_t width,
  const std::uint32_t height
) {
  const float viewport_width = static_cast<float>(std::max(width, 1U));
  const float viewport_height = static_cast<float>(std::max(height, 1U));
  const float half_width_pixels = sprite.half_width_ndc * viewport_width * 0.5F;
  const float half_height_pixels = sprite.half_height_ndc * viewport_height * 0.5F;
  const float cosine = std::cos(sprite.rotation_radians);
  const float sine = std::sin(sprite.rotation_radians);
  constexpr std::array<std::array<float, 4>, 6> corners{{
    {{-1.0F, -1.0F, 0.0F, 0.0F}}, {{1.0F, -1.0F, 1.0F, 0.0F}}, {{1.0F, 1.0F, 1.0F, 1.0F}},
    {{-1.0F, -1.0F, 0.0F, 0.0F}}, {{1.0F, 1.0F, 1.0F, 1.0F}}, {{-1.0F, 1.0F, 0.0F, 1.0F}},
  }};
  for (const auto& corner : corners) {
    const float local_x = corner[0] * half_width_pixels;
    const float local_y = corner[1] * half_height_pixels;
    const float rotated_x = (local_x * cosine) - (local_y * sine);
    const float rotated_y = (local_x * sine) + (local_y * cosine);
    vertices.push_back({
      .x = sprite.center_x_ndc + ((rotated_x * 2.0F) / viewport_width),
      .y = sprite.center_y_ndc + ((rotated_y * 2.0F) / viewport_height),
      .u = corner[2], .v = corner[3],
      .r = sprite.tint_r, .g = sprite.tint_g, .b = sprite.tint_b, .a = sprite.tint_a,
    });
  }
}

void append_model_vertices(
  std::vector<SpriteVertex>& vertices,
  const ModelMesh& mesh,
  const ModelDraw& model,
  const std::uint32_t width,
  const std::uint32_t height
) {
  const float viewport_width = static_cast<float>(std::max(width, 1U));
  const float viewport_height = static_cast<float>(std::max(height, 1U));
  const float cosine = std::cos(model.rotation_radians);
  const float sine = std::sin(model.rotation_radians);
  vertices.reserve(vertices.size() + mesh.vertices.size());
  for (const ModelMeshVertex& source : mesh.vertices) {
    const float local_x = source.x * model.size_pixels * model.roll_scale;
    const float local_y = source.y * model.size_pixels;
    const float rotated_x = (local_x * cosine) - (local_y * sine);
    const float rotated_y = (local_x * sine) + (local_y * cosine);
    const float height_shade = std::clamp(0.90F + (source.height * 0.18F), 0.78F, 1.06F);
    vertices.push_back({
      .x = model.center_x_ndc + ((rotated_x * 2.0F) / viewport_width),
      .y = model.center_y_ndc + ((rotated_y * 2.0F) / viewport_height),
      .u = source.u,
      .v = source.v,
      .r = model.tint_r * height_shade,
      .g = model.tint_g * height_shade,
      .b = model.tint_b * height_shade,
      .a = model.tint_a,
    });
  }
}

[[nodiscard]] std::vector<std::uint32_t> read_spirv(const std::string& path) {
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  if (!stream) {
    throw std::runtime_error("failed to open shader: " + path);
  }
  const std::streamsize byte_count = stream.tellg();
  if (byte_count <= 0 || (byte_count % 4) != 0) {
    throw std::runtime_error("invalid SPIR-V shader: " + path);
  }
  std::vector<std::uint32_t> words(static_cast<std::size_t>(byte_count) / 4U);
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(words.data()), byte_count);
  if (!stream) {
    throw std::runtime_error("failed to read shader: " + path);
  }
  return words;
}

[[nodiscard]] VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
  for (const VkSurfaceFormatKHR format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }
  return formats.front();
}

[[nodiscard]] VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes) {
  return std::ranges::find(modes, VK_PRESENT_MODE_FIFO_KHR) != modes.end() ? VK_PRESENT_MODE_FIFO_KHR : modes.front();
}

}  // namespace

struct VulkanRenderer::Impl {
  explicit Impl(SDL_Window& window) : window_{&window} {
    try {
      create_instance();
      create_surface();
      select_device();
      create_device();
      create_command_resources();
      create_descriptor_resources();
      create_sampler();
      create_textures();
      model_mesh_ = load_obj_model_mesh("assets/models/drone/drone.obj");
      create_swapchain();
      create_sync_resources();
    } catch (...) {
      shutdown();
      throw;
    }
  }

  ~Impl() { shutdown(); }

  void create_instance() {
    Uint32 extension_count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (extensions == nullptr) {
      throw std::runtime_error(std::string{"SDL_Vulkan_GetInstanceExtensions failed: "} + SDL_GetError());
    }
    const VkApplicationInfo app_info{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Hyperverse",
      .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
      .pEngineName = "Hyperverse",
      .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
      .apiVersion = VK_API_VERSION_1_2,
    };
    const VkInstanceCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = extension_count,
      .ppEnabledExtensionNames = extensions,
    };
    vk_check(vkCreateInstance(&create_info, nullptr, &instance_), "vkCreateInstance");
  }

  void create_surface() {
    if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
      throw std::runtime_error(std::string{"SDL_Vulkan_CreateSurface failed: "} + SDL_GetError());
    }
  }

  void select_device() {
    std::uint32_t count = 0;
    vk_check(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
    if (count == 0) {
      throw std::runtime_error("no Vulkan physical device found");
    }
    std::vector<VkPhysicalDevice> devices(count);
    vk_check(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "vkEnumeratePhysicalDevices");
    int best_score = -1;
    for (const VkPhysicalDevice candidate : devices) {
      std::uint32_t queue_count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, nullptr);
      std::vector<VkQueueFamilyProperties> queues(queue_count);
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, queues.data());
      std::uint32_t graphics = invalid_queue;
      std::uint32_t present = invalid_queue;
      for (std::uint32_t index = 0; index < queue_count; ++index) {
        if ((queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
          graphics = index;
        }
        VkBool32 supported = VK_FALSE;
        vk_check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface_, &supported), "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (supported == VK_TRUE) {
          present = index;
        }
      }
      if (graphics == invalid_queue || present == invalid_queue) {
        continue;
      }
      std::uint32_t extension_count = 0;
      vk_check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr), "vkEnumerateDeviceExtensionProperties");
      std::vector<VkExtensionProperties> extensions(extension_count);
      vk_check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, extensions.data()), "vkEnumerateDeviceExtensionProperties");
      const bool has_swapchain = std::ranges::any_of(extensions, [](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
      });
      if (!has_swapchain) {
        continue;
      }
      VkPhysicalDeviceProperties properties{};
      vkGetPhysicalDeviceProperties(candidate, &properties);
      const int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
      if (score > best_score) {
        physical_device_ = candidate;
        graphics_queue_family_ = graphics;
        present_queue_family_ = present;
        best_score = score;
      }
    }
    if (physical_device_ == VK_NULL_HANDLE) {
      throw std::runtime_error("no Vulkan device with graphics, presentation, and swapchain support found");
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device_, &properties);
    SDL_Log("Vulkan adapter: %s", properties.deviceName);
  }

  void create_device() {
    constexpr float priority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queues;
    queues.push_back({
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = graphics_queue_family_,
      .queueCount = 1,
      .pQueuePriorities = &priority,
    });
    if (present_queue_family_ != graphics_queue_family_) {
      queues.push_back({
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = present_queue_family_,
        .queueCount = 1,
        .pQueuePriorities = &priority,
      });
    }
    constexpr std::array extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const VkDeviceCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<std::uint32_t>(queues.size()),
      .pQueueCreateInfos = queues.data(),
      .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
    };
    vk_check(vkCreateDevice(physical_device_, &create_info, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_queue_family_, 0, &present_queue_);
  }

  void create_command_resources() {
    const VkCommandPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphics_queue_family_,
    };
    vk_check(vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_), "vkCreateCommandPool");
    const VkCommandBufferAllocateInfo allocate_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = command_pool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    vk_check(vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer_), "vkAllocateCommandBuffers");
  }

  void create_descriptor_resources() {
    const VkDescriptorSetLayoutBinding binding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    const VkDescriptorSetLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &binding,
    };
    vk_check(vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &descriptor_set_layout_), "vkCreateDescriptorSetLayout");
    const VkPipelineLayoutCreateInfo sprite_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout_,
    };
    vk_check(vkCreatePipelineLayout(device_, &sprite_layout_info, nullptr, &sprite_pipeline_layout_), "vkCreatePipelineLayout");
    const VkPipelineLayoutCreateInfo plain_layout_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    vk_check(vkCreatePipelineLayout(device_, &plain_layout_info, nullptr, &plain_pipeline_layout_), "vkCreatePipelineLayout");
    const VkDescriptorPoolSize pool_size{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 64};
    const VkDescriptorPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 64,
      .poolSizeCount = 1,
      .pPoolSizes = &pool_size,
    };
    vk_check(vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_), "vkCreateDescriptorPool");
  }

  void create_sampler() {
    const VkSamplerCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .maxLod = 0.0F,
    };
    vk_check(vkCreateSampler(device_, &info, nullptr, &sampler_), "vkCreateSampler");
  }

  [[nodiscard]] std::uint32_t memory_type(const std::uint32_t bits, const VkMemoryPropertyFlags flags) const {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &properties);
    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
      if ((bits & (1U << index)) != 0U && (properties.memoryTypes[index].propertyFlags & flags) == flags) {
        return index;
      }
    }
    throw std::runtime_error("no compatible Vulkan memory type found");
  }

  void create_buffer(
    const VkDeviceSize size,
    const VkBufferUsageFlags usage,
    const VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory
  ) const {
    const VkBufferCreateInfo buffer_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    vk_check(vkCreateBuffer(device_, &buffer_info, nullptr, &buffer), "vkCreateBuffer");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &requirements);
    const VkMemoryAllocateInfo memory_info{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = requirements.size,
      .memoryTypeIndex = memory_type(requirements.memoryTypeBits, properties),
    };
    vk_check(vkAllocateMemory(device_, &memory_info, nullptr, &memory), "vkAllocateMemory");
    vk_check(vkBindBufferMemory(device_, buffer, memory, 0), "vkBindBufferMemory");
  }

  [[nodiscard]] VkCommandBuffer begin_one_time_commands() const {
    VkCommandBuffer command = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo allocate_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = command_pool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    vk_check(vkAllocateCommandBuffers(device_, &allocate_info, &command), "vkAllocateCommandBuffers");
    const VkCommandBufferBeginInfo begin_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vk_check(vkBeginCommandBuffer(command, &begin_info), "vkBeginCommandBuffer");
    return command;
  }

  void end_one_time_commands(const VkCommandBuffer command) const {
    vk_check(vkEndCommandBuffer(command), "vkEndCommandBuffer");
    const VkSubmitInfo submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command,
    };
    vk_check(vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE), "vkQueueSubmit");
    vk_check(vkQueueWaitIdle(graphics_queue_), "vkQueueWaitIdle");
    vkFreeCommandBuffers(device_, command_pool_, 1, &command);
  }

  void create_textures() {
    const std::vector<LoadedSprite> sprites = load_sprite_textures();
    textures_.reserve(sprites.size());
    for (const LoadedSprite& sprite : sprites) {
      Texture texture{};
      const VkDeviceSize byte_count = static_cast<VkDeviceSize>(sprite.rgba.size());
      VkBuffer staging = VK_NULL_HANDLE;
      VkDeviceMemory staging_memory = VK_NULL_HANDLE;
      create_buffer(byte_count, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, staging_memory);
      void* mapped = nullptr;
      vk_check(vkMapMemory(device_, staging_memory, 0, byte_count, 0, &mapped), "vkMapMemory");
      std::memcpy(mapped, sprite.rgba.data(), sprite.rgba.size());
      vkUnmapMemory(device_, staging_memory);

      const VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {.width = sprite.width, .height = sprite.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      };
      vk_check(vkCreateImage(device_, &image_info, nullptr, &texture.image), "vkCreateImage");
      VkMemoryRequirements requirements{};
      vkGetImageMemoryRequirements(device_, texture.image, &requirements);
      const VkMemoryAllocateInfo memory_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
      };
      vk_check(vkAllocateMemory(device_, &memory_info, nullptr, &texture.memory), "vkAllocateMemory");
      vk_check(vkBindImageMemory(device_, texture.image, texture.memory, 0), "vkBindImageMemory");

      const VkCommandBuffer command = begin_one_time_commands();
      const VkImageMemoryBarrier to_transfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
      };
      vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                           0, nullptr, 0, nullptr, 1, &to_transfer);
      const VkBufferImageCopy copy{
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .imageExtent = {.width = sprite.width, .height = sprite.height, .depth = 1},
      };
      vkCmdCopyBufferToImage(command, staging, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
      const VkImageMemoryBarrier to_shader{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
      };
      vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                           0, nullptr, 0, nullptr, 1, &to_shader);
      end_one_time_commands(command);
      vkDestroyBuffer(device_, staging, nullptr);
      vkFreeMemory(device_, staging_memory, nullptr);

      const VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
      };
      vk_check(vkCreateImageView(device_, &view_info, nullptr, &texture.view), "vkCreateImageView");
      const VkDescriptorSetAllocateInfo set_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_set_layout_,
      };
      vk_check(vkAllocateDescriptorSets(device_, &set_info, &texture.descriptor), "vkAllocateDescriptorSets");
      const VkDescriptorImageInfo descriptor_image{
        .sampler = sampler_,
        .imageView = texture.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      };
      const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = texture.descriptor,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &descriptor_image,
      };
      vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
      textures_.push_back(texture);
    }
  }

  void query_surface(
    VkSurfaceCapabilitiesKHR& capabilities,
    std::vector<VkSurfaceFormatKHR>& formats,
    std::vector<VkPresentModeKHR>& modes
  ) const {
    vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    std::uint32_t format_count = 0;
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    formats.resize(format_count);
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    std::uint32_t mode_count = 0;
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &mode_count, nullptr), "vkGetPhysicalDeviceSurfacePresentModesKHR");
    modes.resize(mode_count);
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &mode_count, modes.data()), "vkGetPhysicalDeviceSurfacePresentModesKHR");
    if (formats.empty() || modes.empty()) {
      throw std::runtime_error("Vulkan surface has no usable formats or presentation modes");
    }
  }

  void create_swapchain() {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> modes;
    query_surface(capabilities, formats, modes);
    const VkSurfaceFormatKHR selected_format = choose_surface_format(formats);
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
      extent_ = capabilities.currentExtent;
    } else {
      int pixel_width = 0;
      int pixel_height = 0;
      SDL_GetWindowSizeInPixels(window_, &pixel_width, &pixel_height);
      extent_.width = std::clamp(static_cast<std::uint32_t>(std::max(pixel_width, 1)), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
      extent_.height = std::clamp(static_cast<std::uint32_t>(std::max(pixel_height, 1)), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    std::uint32_t image_count = capabilities.minImageCount + 1U;
    if (capabilities.maxImageCount > 0U) {
      image_count = std::min(image_count, capabilities.maxImageCount);
    }
    const std::array queue_families{graphics_queue_family_, present_queue_family_};
    const bool split_queues = graphics_queue_family_ != present_queue_family_;
    const VkSwapchainCreateInfoKHR info{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface_,
      .minImageCount = image_count,
      .imageFormat = selected_format.format,
      .imageColorSpace = selected_format.colorSpace,
      .imageExtent = extent_,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = split_queues ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = split_queues ? 2U : 0U,
      .pQueueFamilyIndices = split_queues ? queue_families.data() : nullptr,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = choose_present_mode(modes),
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
    };
    vk_check(vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_), "vkCreateSwapchainKHR");
    swapchain_format_ = selected_format.format;
    vk_check(vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr), "vkGetSwapchainImagesKHR");
    swapchain_images_.resize(image_count);
    vk_check(vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data()), "vkGetSwapchainImagesKHR");
    create_render_pass();
    create_pipelines();
    create_framebuffers();
  }

  void create_render_pass() {
    const VkAttachmentDescription attachment{
      .format = swapchain_format_,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    const VkAttachmentReference reference{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkSubpassDescription subpass{
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &reference,
    };
    const VkSubpassDependency dependency{
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    const VkRenderPassCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 1,
      .pDependencies = &dependency,
    };
    vk_check(vkCreateRenderPass(device_, &info, nullptr, &render_pass_), "vkCreateRenderPass");
  }

  [[nodiscard]] VkShaderModule shader_module(const std::string& filename) const {
    const std::vector<std::uint32_t> code = read_spirv("shaders/" + filename + ".spv");
    const VkShaderModuleCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = code.size() * sizeof(std::uint32_t),
      .pCode = code.data(),
    };
    VkShaderModule module = VK_NULL_HANDLE;
    vk_check(vkCreateShaderModule(device_, &info, nullptr, &module), "vkCreateShaderModule");
    return module;
  }

  [[nodiscard]] VkPipeline create_pipeline(
    const char* vertex_shader,
    const char* fragment_shader,
    const VkPipelineLayout layout,
    const VkPrimitiveTopology topology,
    const std::uint32_t stride,
    const std::vector<VkVertexInputAttributeDescription>& attributes,
    const bool additive = false
  ) const {
    const VkShaderModule vertex = shader_module(vertex_shader);
    const VkShaderModule fragment = shader_module(fragment_shader);
    const std::array stages{
      VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertex, .pName = "main"},
      VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragment, .pName = "main"},
    };
    const VkVertexInputBindingDescription binding{.binding = 0, .stride = stride, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    const VkPipelineVertexInputStateCreateInfo vertex_input{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding,
      .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size()),
      .pVertexAttributeDescriptions = attributes.data(),
    };
    const VkPipelineInputAssemblyStateCreateInfo assembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = topology,
    };
    const VkPipelineViewportStateCreateInfo viewport{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
    };
    const VkPipelineRasterizationStateCreateInfo rasterization{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0F,
    };
    const VkPipelineMultisampleStateCreateInfo multisample{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    const VkPipelineColorBlendAttachmentState blend{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = additive ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = additive ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo blend_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &blend,
    };
    constexpr std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
      .pDynamicStates = dynamic_states.data(),
    };
    const VkGraphicsPipelineCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = static_cast<std::uint32_t>(stages.size()),
      .pStages = stages.data(),
      .pVertexInputState = &vertex_input,
      .pInputAssemblyState = &assembly,
      .pViewportState = &viewport,
      .pRasterizationState = &rasterization,
      .pMultisampleState = &multisample,
      .pColorBlendState = &blend_state,
      .pDynamicState = &dynamic,
      .layout = layout,
      .renderPass = render_pass_,
      .subpass = 0,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    vkDestroyShaderModule(device_, fragment, nullptr);
    vkDestroyShaderModule(device_, vertex, nullptr);
    vk_check(result, "vkCreateGraphicsPipelines");
    return pipeline;
  }

  void create_pipelines() {
    const std::vector<VkVertexInputAttributeDescription> sprite_attributes{
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(SpriteVertex, x)},
      {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(SpriteVertex, u)},
      {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(SpriteVertex, r)},
    };
    const std::vector<VkVertexInputAttributeDescription> color_attributes{
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(ColorVertex, x)},
      {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(ColorVertex, r)},
    };
    const std::vector<VkVertexInputAttributeDescription> triangle_attributes{
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(TriangleVertex, x)},
      {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(TriangleVertex, u)},
      {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(TriangleVertex, r)},
    };
    const std::vector<VkVertexInputAttributeDescription> trail_attributes{
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(TrailVertex, x)},
      {.location = 1, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(TrailVertex, normalized_age)},
      {.location = 2, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(TrailVertex, intensity)},
      {.location = 3, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(TrailVertex, side)},
    };
    sprite_pipeline_ = create_pipeline("sprite.vert", "sprite.frag", sprite_pipeline_layout_, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, sizeof(SpriteVertex), sprite_attributes);
    color_pipeline_ = create_pipeline("color.vert", "color.frag", plain_pipeline_layout_, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, sizeof(ColorVertex), color_attributes);
    line_pipeline_ = create_pipeline("color.vert", "color.frag", plain_pipeline_layout_, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, sizeof(ColorVertex), color_attributes);
    triangle_pipeline_ = create_pipeline("triangle.vert", "triangle.frag", plain_pipeline_layout_, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, sizeof(TriangleVertex), triangle_attributes);
    trail_pipeline_ = create_pipeline("trail.vert", "trail.frag", plain_pipeline_layout_, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, sizeof(TrailVertex), trail_attributes, true);
  }

  void create_framebuffers() {
    swapchain_views_.resize(swapchain_images_.size());
    framebuffers_.resize(swapchain_images_.size());
    for (std::size_t index = 0; index < swapchain_images_.size(); ++index) {
      const VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = swapchain_images_[index],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain_format_,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
      };
      vk_check(vkCreateImageView(device_, &view_info, nullptr, &swapchain_views_[index]), "vkCreateImageView");
      const VkFramebufferCreateInfo framebuffer_info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass_,
        .attachmentCount = 1,
        .pAttachments = &swapchain_views_[index],
        .width = extent_.width,
        .height = extent_.height,
        .layers = 1,
      };
      vk_check(vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffers_[index]), "vkCreateFramebuffer");
    }
  }

  void create_sync_resources() {
    const VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vk_check(vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_), "vkCreateSemaphore");
    vk_check(vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_), "vkCreateSemaphore");
    const VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    vk_check(vkCreateFence(device_, &fence_info, nullptr, &frame_fence_), "vkCreateFence");
  }

  void destroy_swapchain() {
    for (const VkFramebuffer framebuffer : framebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
    framebuffers_.clear();
    for (const VkImageView view : swapchain_views_) vkDestroyImageView(device_, view, nullptr);
    swapchain_views_.clear();
    for (const VkPipeline pipeline : {sprite_pipeline_, color_pipeline_, line_pipeline_, triangle_pipeline_, trail_pipeline_}) {
      if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipeline, nullptr);
    }
    sprite_pipeline_ = color_pipeline_ = line_pipeline_ = triangle_pipeline_ = trail_pipeline_ = VK_NULL_HANDLE;
    if (render_pass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, render_pass_, nullptr);
    render_pass_ = VK_NULL_HANDLE;
    if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
    swapchain_images_.clear();
  }

  void recreate_swapchain() {
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window_, &width, &height);
    if (width <= 0 || height <= 0) {
      return;
    }
    vk_check(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle");
    destroy_swapchain();
    create_swapchain();
    swapchain_dirty_ = false;
  }

  struct FrameBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
  };

  template <typename Vertex>
  void draw_vertices(const VkPipeline pipeline, const std::vector<Vertex>& vertices, const VkDescriptorSet descriptor = VK_NULL_HANDLE) {
    if (vertices.empty()) return;
    FrameBuffer allocation{};
    const VkDeviceSize size = sizeof(Vertex) * vertices.size();
    create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocation.buffer, allocation.memory);
    void* mapped = nullptr;
    vk_check(vkMapMemory(device_, allocation.memory, 0, size, 0, &mapped), "vkMapMemory");
    std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(size));
    vkUnmapMemory(device_, allocation.memory);
    frame_buffers_.push_back(allocation);
    vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    if (descriptor != VK_NULL_HANDLE) {
      vkCmdBindDescriptorSets(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, sprite_pipeline_layout_, 0, 1, &descriptor, 0, nullptr);
    }
    constexpr VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(command_buffer_, 0, 1, &allocation.buffer, &offset);
    vkCmdDraw(command_buffer_, static_cast<std::uint32_t>(vertices.size()), 1, 0, 0);
  }

  void record_frame(const SpriteFrame& frame, const std::uint32_t image_index) {
    const VkCommandBufferBeginInfo command_begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vk_check(vkBeginCommandBuffer(command_buffer_, &command_begin), "vkBeginCommandBuffer");
    const RenderColor clear = make_clear_color(frame.state);
    const VkClearValue clear_value{.color = {.float32 = {clear.r, clear.g, clear.b, clear.a}}};
    const VkRenderPassBeginInfo render_begin{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = render_pass_,
      .framebuffer = framebuffers_[image_index],
      .renderArea = {.offset = {0, 0}, .extent = extent_},
      .clearValueCount = 1,
      .pClearValues = &clear_value,
    };
    vkCmdBeginRenderPass(command_buffer_, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport{.x = 0.0F, .y = 0.0F, .width = static_cast<float>(extent_.width), .height = static_cast<float>(extent_.height), .minDepth = 0.0F, .maxDepth = 1.0F};
    const VkRect2D scissor{.offset = {0, 0}, .extent = extent_};
    vkCmdSetViewport(command_buffer_, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer_, 0, 1, &scissor);

    std::vector<ColorVertex> stars;
    stars.reserve(frame.stars.size() * 6U);
    for (const StarDraw& star : frame.stars) {
      const float left = star.x_ndc - star.half_size_x_ndc;
      const float right = star.x_ndc + star.half_size_x_ndc;
      const float top = star.y_ndc - star.half_size_y_ndc;
      const float bottom = star.y_ndc + star.half_size_y_ndc;
      const ColorVertex a{left, top, star.r, star.g, star.b, star.a};
      const ColorVertex b{right, top, star.r, star.g, star.b, star.a};
      const ColorVertex c{left, bottom, star.r, star.g, star.b, star.a};
      const ColorVertex d{right, bottom, star.r, star.g, star.b, star.a};
      stars.insert(stars.end(), {a, b, c, b, d, c});
    }
    draw_vertices(color_pipeline_, stars);

    std::vector<TriangleVertex> triangles;
    triangles.reserve(frame.triangles.size() * 3U);
    const auto append_triangle = [&triangles](const TriangleVertexDraw& vertex) {
      triangles.push_back({vertex.x_ndc, vertex.y_ndc, vertex.u, vertex.v, vertex.r, vertex.g, vertex.b, vertex.a});
    };
    for (const TriangleDraw& triangle : frame.triangles) {
      append_triangle(triangle.a); append_triangle(triangle.b); append_triangle(triangle.c);
    }
    draw_vertices(triangle_pipeline_, triangles);

    std::vector<TrailVertex> trails;
    trails.reserve(frame.engine_trails.size());
    for (const EngineTrailVertexDraw& vertex : frame.engine_trails) {
      trails.push_back({vertex.x_ndc, vertex.y_ndc, vertex.normalized_age, vertex.intensity, vertex.side});
    }
    draw_vertices(trail_pipeline_, trails);

    for (const ModelTexture texture : {ModelTexture::DroneFriendly, ModelTexture::DroneHostile}) {
      const std::size_t texture_index = static_cast<std::size_t>(SpriteTexture::Count) + static_cast<std::size_t>(texture);
      if (texture_index >= textures_.size()) continue;
      std::vector<SpriteVertex> vertices;
      for (const ModelDraw& model : frame.models) {
        if (model.texture == texture) append_model_vertices(vertices, model_mesh_, model, extent_.width, extent_.height);
      }
      draw_vertices(sprite_pipeline_, vertices, textures_[texture_index].descriptor);
    }

    for (const SpriteDraw& sprite : frame.sprites) {
      const std::size_t texture_index = static_cast<std::size_t>(sprite.texture);
      if (texture_index >= textures_.size()) continue;
      std::vector<SpriteVertex> vertices;
      vertices.reserve(6);
      append_sprite_vertices(vertices, sprite, extent_.width, extent_.height);
      draw_vertices(sprite_pipeline_, vertices, textures_[texture_index].descriptor);
    }

    std::vector<ColorVertex> lines;
    lines.reserve(frame.lines.size() * 2U);
    for (const LineDraw& line : frame.lines) {
      lines.push_back({line.start_x_ndc, line.start_y_ndc, line.r, line.g, line.b, line.a});
      lines.push_back({line.end_x_ndc, line.end_y_ndc, line.r, line.g, line.b, line.a});
    }
    draw_vertices(line_pipeline_, lines);
    vkCmdEndRenderPass(command_buffer_);
    vk_check(vkEndCommandBuffer(command_buffer_), "vkEndCommandBuffer");
  }

  void release_frame_buffers() {
    for (const FrameBuffer& allocation : frame_buffers_) {
      vkDestroyBuffer(device_, allocation.buffer, nullptr);
      vkFreeMemory(device_, allocation.memory, nullptr);
    }
    frame_buffers_.clear();
  }

  void draw_frame(const SpriteFrame& frame) {
    if (swapchain_dirty_) recreate_swapchain();
    if (swapchain_dirty_ || extent_.width == 0 || extent_.height == 0) return;
    vk_check(vkWaitForFences(device_, 1, &frame_fence_, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    release_frame_buffers();
    std::uint32_t image_index = 0;
    const VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, image_available_, VK_NULL_HANDLE, &image_index);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
      swapchain_dirty_ = true;
      return;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) vk_check(acquire, "vkAcquireNextImageKHR");
    vk_check(vkResetFences(device_, 1, &frame_fence_), "vkResetFences");
    vk_check(vkResetCommandBuffer(command_buffer_, 0), "vkResetCommandBuffer");
    record_frame(frame, image_index);
    constexpr VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSubmitInfo submit{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &image_available_,
      .pWaitDstStageMask = &wait_stage,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer_,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &render_finished_,
    };
    vk_check(vkQueueSubmit(graphics_queue_, 1, &submit, frame_fence_), "vkQueueSubmit");
    const VkPresentInfoKHR present{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &render_finished_,
      .swapchainCount = 1,
      .pSwapchains = &swapchain_,
      .pImageIndices = &image_index,
    };
    const VkResult result = vkQueuePresentKHR(present_queue_, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || acquire == VK_SUBOPTIMAL_KHR) {
      swapchain_dirty_ = true;
    } else {
      vk_check(result, "vkQueuePresentKHR");
    }
  }

  void refresh_extent() { swapchain_dirty_ = true; }
  void wait_idle() const { if (device_ != VK_NULL_HANDLE) vk_check(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle"); }
  [[nodiscard]] std::uint32_t width() const { return extent_.width; }
  [[nodiscard]] std::uint32_t height() const { return extent_.height; }

  void shutdown() noexcept {
    if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
    release_frame_buffers();
    destroy_swapchain();
    for (const Texture& texture : textures_) {
      if (texture.view != VK_NULL_HANDLE) vkDestroyImageView(device_, texture.view, nullptr);
      if (texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, texture.image, nullptr);
      if (texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, texture.memory, nullptr);
    }
    textures_.clear();
    if (sampler_ != VK_NULL_HANDLE) vkDestroySampler(device_, sampler_, nullptr);
    if (descriptor_pool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
    if (sprite_pipeline_layout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, sprite_pipeline_layout_, nullptr);
    if (plain_pipeline_layout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, plain_pipeline_layout_, nullptr);
    if (descriptor_set_layout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
    if (frame_fence_ != VK_NULL_HANDLE) vkDestroyFence(device_, frame_fence_, nullptr);
    if (render_finished_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, render_finished_, nullptr);
    if (image_available_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, image_available_, nullptr);
    if (command_pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, command_pool_, nullptr);
    if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
    if (surface_ != VK_NULL_HANDLE) SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
    if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    surface_ = VK_NULL_HANDLE;
    instance_ = VK_NULL_HANDLE;
  }

  struct Texture {
    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkDescriptorSet descriptor{VK_NULL_HANDLE};
  };

  SDL_Window* window_{};
  VkInstance instance_{VK_NULL_HANDLE};
  VkSurfaceKHR surface_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  std::uint32_t graphics_queue_family_{invalid_queue};
  std::uint32_t present_queue_family_{invalid_queue};
  VkQueue graphics_queue_{VK_NULL_HANDLE};
  VkQueue present_queue_{VK_NULL_HANDLE};
  VkCommandPool command_pool_{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
  VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  VkFormat swapchain_format_{VK_FORMAT_UNDEFINED};
  VkExtent2D extent_{1280, 720};
  std::vector<VkImage> swapchain_images_{};
  std::vector<VkImageView> swapchain_views_{};
  std::vector<VkFramebuffer> framebuffers_{};
  VkRenderPass render_pass_{VK_NULL_HANDLE};
  VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
  VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
  VkPipelineLayout sprite_pipeline_layout_{VK_NULL_HANDLE};
  VkPipelineLayout plain_pipeline_layout_{VK_NULL_HANDLE};
  VkSampler sampler_{VK_NULL_HANDLE};
  VkPipeline sprite_pipeline_{VK_NULL_HANDLE};
  VkPipeline color_pipeline_{VK_NULL_HANDLE};
  VkPipeline line_pipeline_{VK_NULL_HANDLE};
  VkPipeline triangle_pipeline_{VK_NULL_HANDLE};
  VkPipeline trail_pipeline_{VK_NULL_HANDLE};
  VkSemaphore image_available_{VK_NULL_HANDLE};
  VkSemaphore render_finished_{VK_NULL_HANDLE};
  VkFence frame_fence_{VK_NULL_HANDLE};
  std::vector<Texture> textures_{};
  ModelMesh model_mesh_{};
  std::vector<FrameBuffer> frame_buffers_{};
  bool swapchain_dirty_{false};
};

VulkanRenderer::VulkanRenderer(SDL_Window& window) : impl_{std::make_unique<Impl>(window)} {}
VulkanRenderer::~VulkanRenderer() = default;
void VulkanRenderer::draw_frame(const FrameSnapshot& frame) { draw_frame(SpriteFrame{.state = frame}); }
void VulkanRenderer::draw_frame(const SpriteFrame& frame) { impl_->draw_frame(frame); }
void VulkanRenderer::refresh_extent() { impl_->refresh_extent(); }
void VulkanRenderer::wait_idle() const { impl_->wait_idle(); }
std::uint32_t VulkanRenderer::width() const { return impl_->width(); }
std::uint32_t VulkanRenderer::height() const { return impl_->height(); }

}  // namespace hyperverse

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
