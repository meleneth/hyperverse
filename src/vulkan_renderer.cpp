#include "hyperverse/vulkan_renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <png.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

struct SpritePushConstants {
  float center_x{0.0F};
  float center_y{0.0F};
  float half_width{0.0F};
  float half_height{0.0F};
  float rotation_radians{0.0F};
  float padding0{0.0F};
  float padding1{0.0F};
  float padding2{0.0F};
  float tint_r{1.0F};
  float tint_g{1.0F};
  float tint_b{1.0F};
  float tint_a{1.0F};
};

struct LinePushConstants {
  float start_x{0.0F};
  float start_y{0.0F};
  float end_x{0.0F};
  float end_y{0.0F};
  float r{1.0F};
  float g{1.0F};
  float b{1.0F};
  float a{1.0F};
};

void check(VkResult result, const char* message) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(message);
  }
}

struct LoadedPng {
  std::uint32_t width{0};
  std::uint32_t height{0};
  std::vector<std::uint8_t> rgba{};
};

[[nodiscard]] std::vector<char> read_file(const std::filesystem::path& path) {
  std::ifstream file{path, std::ios::ate | std::ios::binary};
  if (!file) {
    throw std::runtime_error("failed to open file: " + path.string());
  }

  const std::streamsize size = file.tellg();
  std::vector<char> buffer(static_cast<std::size_t>(size));
  file.seekg(0);
  file.read(buffer.data(), size);
  return buffer;
}

[[nodiscard]] LoadedPng load_png_rgba(const std::filesystem::path& path) {
  FILE* file = std::fopen(path.string().c_str(), "rb");
  if (file == nullptr) {
    throw std::runtime_error("failed to open png: " + path.string());
  }

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = png != nullptr ? png_create_info_struct(png) : nullptr;
  if (png == nullptr || info == nullptr) {
    png_destroy_read_struct(&png, nullptr, nullptr);
    std::fclose(file);
    throw std::runtime_error("failed to allocate png decoder");
  }

  if (setjmp(png_jmpbuf(png)) != 0) {
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(file);
    throw std::runtime_error("failed to decode png: " + path.string());
  }

  png_init_io(png, file);
  png_read_info(png, info);

  const png_uint_32 width = png_get_image_width(png, info);
  const png_uint_32 height = png_get_image_height(png, info);
  const png_byte color_type = png_get_color_type(png, info);
  const png_byte bit_depth = png_get_bit_depth(png, info);

  if (bit_depth == 16) {
    png_set_strip_16(png);
  }
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png);
  }
  if (png_get_valid(png, info, PNG_INFO_tRNS) != 0) {
    png_set_tRNS_to_alpha(png);
  }
  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png);
  }

  png_read_update_info(png, info);

  LoadedPng image{
    .width = static_cast<std::uint32_t>(width),
    .height = static_cast<std::uint32_t>(height),
    .rgba = std::vector<std::uint8_t>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U),
  };

  std::vector<png_bytep> rows(height);
  for (png_uint_32 row = 0; row < height; ++row) {
    rows[row] = image.rgba.data() + (static_cast<std::size_t>(row) * width * 4U);
  }
  png_read_image(png, rows.data());

  png_destroy_read_struct(&png, &info, nullptr);
  std::fclose(file);
  return image;
}

[[nodiscard]] VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code) {
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = code.size();
  create_info.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

  VkShaderModule shader = VK_NULL_HANDLE;
  check(vkCreateShaderModule(device, &create_info, nullptr, &shader), "create shader module");
  return shader;
}

}  // namespace

namespace hyperverse {

VulkanRenderer::VulkanRenderer(SDL_Window& window) {
  create_instance(window);
  create_surface(window);
  pick_physical_device();
  create_logical_device();
  create_swapchain(window);
  create_image_views();
  create_render_pass();
  create_descriptor_set_layout();
  create_graphics_pipeline();
  create_line_pipeline();
  create_framebuffers();
  create_command_pool();
  create_texture_resources();
  create_texture_descriptor_sets();
  create_command_buffers();
  create_sync_objects();
}

VulkanRenderer::~VulkanRenderer() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
  }

  if (in_flight_ != VK_NULL_HANDLE) {
    vkDestroyFence(device_, in_flight_, nullptr);
  }
  if (render_finished_ != VK_NULL_HANDLE) {
    vkDestroySemaphore(device_, render_finished_, nullptr);
  }
  if (image_available_ != VK_NULL_HANDLE) {
    vkDestroySemaphore(device_, image_available_, nullptr);
  }
  if (command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, command_pool_, nullptr);
  }
  if (descriptor_pool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
  }
  if (texture_sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device_, texture_sampler_, nullptr);
  }
  for (TextureResource& texture : textures_) {
    if (texture.view != VK_NULL_HANDLE) {
      vkDestroyImageView(device_, texture.view, nullptr);
    }
    if (texture.image != VK_NULL_HANDLE) {
      vkDestroyImage(device_, texture.image, nullptr);
    }
    if (texture.memory != VK_NULL_HANDLE) {
      vkFreeMemory(device_, texture.memory, nullptr);
    }
  }
  if (graphics_pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, graphics_pipeline_, nullptr);
  }
  if (line_pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, line_pipeline_, nullptr);
  }
  if (pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
  }
  if (line_pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, line_pipeline_layout_, nullptr);
  }
  if (descriptor_set_layout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
  }
  for (VkFramebuffer framebuffer : framebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }
  if (render_pass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, render_pass_, nullptr);
  }
  for (VkImageView image_view : swapchain_image_views_) {
    vkDestroyImageView(device_, image_view, nullptr);
  }
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  }
  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
  }
  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
  }
}

void VulkanRenderer::draw_frame(const VulkanFrameSnapshot& frame) {
  draw_frame(SpriteFrame{.state = frame});
}

void VulkanRenderer::draw_frame(const SpriteFrame& frame) {
  check(vkWaitForFences(device_, 1, &in_flight_, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "wait fence");
  check(vkResetFences(device_, 1, &in_flight_), "reset fence");

  std::uint32_t image_index = 0;
  VkResult acquire_result =
    vkAcquireNextImageKHR(device_, swapchain_, std::numeric_limits<std::uint64_t>::max(), image_available_, VK_NULL_HANDLE, &image_index);
  if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swapchain image");
  }

  record_command_buffer(image_index, frame);

  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &image_available_;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffers_.at(image_index);
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &render_finished_;

  check(vkQueueSubmit(graphics_queue_, 1, &submit_info, in_flight_), "submit clear command buffer");

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished_;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &image_index;

  VkResult present_result = vkQueuePresentKHR(graphics_queue_, &present_info);
  if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to present swapchain image");
  }
}

void VulkanRenderer::wait_idle() const {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
  }
}

std::uint32_t VulkanRenderer::width() const {
  return swapchain_extent_.width;
}

std::uint32_t VulkanRenderer::height() const {
  return swapchain_extent_.height;
}

void VulkanRenderer::create_instance(SDL_Window& window) {
  std::uint32_t extension_count = 0;
  const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
  if (extensions == nullptr || extension_count == 0) {
    throw std::runtime_error(std::string{"SDL_Vulkan_GetInstanceExtensions failed: "} + SDL_GetError());
  }

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Hyperverse";
  app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.pEngineName = "Hyperverse";
  app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = extension_count;
  create_info.ppEnabledExtensionNames = extensions;

  (void)window;
  check(vkCreateInstance(&create_info, nullptr, &instance_), "create Vulkan instance");
}

void VulkanRenderer::create_surface(SDL_Window& window) {
  if (!SDL_Vulkan_CreateSurface(&window, instance_, nullptr, &surface_)) {
    throw std::runtime_error(std::string{"SDL_Vulkan_CreateSurface failed: "} + SDL_GetError());
  }
}

void VulkanRenderer::pick_physical_device() {
  std::uint32_t device_count = 0;
  check(vkEnumeratePhysicalDevices(instance_, &device_count, nullptr), "count Vulkan physical devices");
  if (device_count == 0) {
    throw std::runtime_error("no Vulkan physical device available");
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  check(vkEnumeratePhysicalDevices(instance_, &device_count, devices.data()), "enumerate Vulkan physical devices");

  for (VkPhysicalDevice device : devices) {
    try {
      graphics_queue_family_ = find_graphics_present_queue_family(device);
      physical_device_ = device;
      return;
    } catch (const std::runtime_error&) {
    }
  }

  throw std::runtime_error("no Vulkan device supports graphics and presentation");
}

void VulkanRenderer::create_logical_device() {
  const float queue_priority = 1.0F;
  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = graphics_queue_family_;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  const std::array device_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.queueCreateInfoCount = 1;
  create_info.pQueueCreateInfos = &queue_create_info;
  create_info.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size());
  create_info.ppEnabledExtensionNames = device_extensions.data();

  check(vkCreateDevice(physical_device_, &create_info, nullptr, &device_), "create Vulkan logical device");
  vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
}

void VulkanRenderer::create_swapchain(SDL_Window& window) {
  VkSurfaceCapabilitiesKHR capabilities{};
  check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities), "get surface capabilities");

  std::uint32_t format_count = 0;
  check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr), "count surface formats");
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data()), "get surface formats");

  std::uint32_t present_mode_count = 0;
  check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, nullptr), "count present modes");
  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  check(
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, present_modes.data()),
    "get present modes"
  );

  const VkSurfaceFormatKHR surface_format = choose_surface_format(formats);
  const VkPresentModeKHR present_mode = choose_present_mode(present_modes);
  const VkExtent2D extent = choose_extent(window, capabilities);

  std::uint32_t image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
    image_count = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface_;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.preTransform = capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;

  check(vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_), "create swapchain");

  check(vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr), "count swapchain images");
  swapchain_images_.resize(image_count);
  check(vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data()), "get swapchain images");

  swapchain_format_ = surface_format.format;
  swapchain_extent_ = extent;
}

void VulkanRenderer::create_image_views() {
  swapchain_image_views_.reserve(swapchain_images_.size());

  for (VkImage image : swapchain_images_) {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = swapchain_format_;
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    VkImageView image_view = VK_NULL_HANDLE;
    check(vkCreateImageView(device_, &create_info, nullptr, &image_view), "create swapchain image view");
    swapchain_image_views_.push_back(image_view);
  }
}

void VulkanRenderer::create_render_pass() {
  VkAttachmentDescription color_attachment{};
  color_attachment.format = swapchain_format_;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  create_info.attachmentCount = 1;
  create_info.pAttachments = &color_attachment;
  create_info.subpassCount = 1;
  create_info.pSubpasses = &subpass;
  create_info.dependencyCount = 1;
  create_info.pDependencies = &dependency;

  check(vkCreateRenderPass(device_, &create_info, nullptr, &render_pass_), "create render pass");
}

void VulkanRenderer::create_descriptor_set_layout() {
  VkDescriptorSetLayoutBinding sampler_binding{};
  sampler_binding.binding = 0;
  sampler_binding.descriptorCount = 1;
  sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  create_info.bindingCount = 1;
  create_info.pBindings = &sampler_binding;

  check(vkCreateDescriptorSetLayout(device_, &create_info, nullptr, &descriptor_set_layout_), "create sprite descriptor set layout");
}

void VulkanRenderer::create_graphics_pipeline() {
  const std::vector<char> vertex_code = read_file("assets/shaders/sprite.vert.spv");
  const std::vector<char> fragment_code = read_file("assets/shaders/sprite.frag.spv");

  VkShaderModule vertex_shader = create_shader_module(device_, vertex_code);
  VkShaderModule fragment_shader = create_shader_module(device_, fragment_code);

  VkPipelineShaderStageCreateInfo vertex_stage{};
  vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_stage.module = vertex_shader;
  vertex_stage.pName = "main";

  VkPipelineShaderStageCreateInfo fragment_stage{};
  fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragment_stage.module = fragment_shader;
  fragment_stage.pName = "main";

  const std::array stages{vertex_stage, fragment_stage};

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkViewport viewport{};
  viewport.x = 0.0F;
  viewport.y = 0.0F;
  viewport.width = static_cast<float>(swapchain_extent_.width);
  viewport.height = static_cast<float>(swapchain_extent_.height);
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchain_extent_;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0F;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_TRUE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  VkPushConstantRange push_range{};
  push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  push_range.offset = 0;
  push_range.size = sizeof(SpritePushConstants);

  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.setLayoutCount = 1;
  layout_info.pSetLayouts = &descriptor_set_layout_;
  layout_info.pushConstantRangeCount = 1;
  layout_info.pPushConstantRanges = &push_range;

  check(vkCreatePipelineLayout(device_, &layout_info, nullptr, &pipeline_layout_), "create sprite pipeline layout");

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = static_cast<std::uint32_t>(stages.size());
  pipeline_info.pStages = stages.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.layout = pipeline_layout_;
  pipeline_info.renderPass = render_pass_;
  pipeline_info.subpass = 0;

  check(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline_), "create sprite pipeline");

  vkDestroyShaderModule(device_, fragment_shader, nullptr);
  vkDestroyShaderModule(device_, vertex_shader, nullptr);
}

void VulkanRenderer::create_line_pipeline() {
  const std::vector<char> vertex_code = read_file("assets/shaders/line.vert.spv");
  const std::vector<char> fragment_code = read_file("assets/shaders/line.frag.spv");

  VkShaderModule vertex_shader = create_shader_module(device_, vertex_code);
  VkShaderModule fragment_shader = create_shader_module(device_, fragment_code);

  VkPipelineShaderStageCreateInfo vertex_stage{};
  vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_stage.module = vertex_shader;
  vertex_stage.pName = "main";

  VkPipelineShaderStageCreateInfo fragment_stage{};
  fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragment_stage.module = fragment_shader;
  fragment_stage.pName = "main";

  const std::array stages{vertex_stage, fragment_stage};

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

  VkViewport viewport{};
  viewport.x = 0.0F;
  viewport.y = 0.0F;
  viewport.width = static_cast<float>(swapchain_extent_.width);
  viewport.height = static_cast<float>(swapchain_extent_.height);
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchain_extent_;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0F;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_TRUE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  VkPushConstantRange push_range{};
  push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  push_range.offset = 0;
  push_range.size = sizeof(LinePushConstants);

  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.pushConstantRangeCount = 1;
  layout_info.pPushConstantRanges = &push_range;

  check(vkCreatePipelineLayout(device_, &layout_info, nullptr, &line_pipeline_layout_), "create line pipeline layout");

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = static_cast<std::uint32_t>(stages.size());
  pipeline_info.pStages = stages.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.layout = line_pipeline_layout_;
  pipeline_info.renderPass = render_pass_;
  pipeline_info.subpass = 0;

  check(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &line_pipeline_), "create line pipeline");

  vkDestroyShaderModule(device_, fragment_shader, nullptr);
  vkDestroyShaderModule(device_, vertex_shader, nullptr);
}

void VulkanRenderer::create_framebuffers() {
  framebuffers_.reserve(swapchain_image_views_.size());

  for (VkImageView image_view : swapchain_image_views_) {
    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = render_pass_;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &image_view;
    create_info.width = swapchain_extent_.width;
    create_info.height = swapchain_extent_.height;
    create_info.layers = 1;

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    check(vkCreateFramebuffer(device_, &create_info, nullptr, &framebuffer), "create framebuffer");
    framebuffers_.push_back(framebuffer);
  }
}

void VulkanRenderer::create_command_pool() {
  VkCommandPoolCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  create_info.queueFamilyIndex = graphics_queue_family_;

  check(vkCreateCommandPool(device_, &create_info, nullptr, &command_pool_), "create command pool");
}

void VulkanRenderer::create_texture_resources() {
  const std::array paths{
    std::filesystem::path{"assets/sector7/sprites/ship.png"},
    std::filesystem::path{"assets/sector7/sprites/rock1.png"},
    std::filesystem::path{"assets/sector7/sprites/reticle.png"},
    std::filesystem::path{"assets/sector7/sprites/laser.png"},
  };

  textures_.resize(paths.size());
  for (std::size_t index = 0; index < paths.size(); ++index) {
    const LoadedPng image = load_png_rgba(paths[index]);
    const VkDeviceSize image_size = static_cast<VkDeviceSize>(image.rgba.size());

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    create_buffer(
      image_size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      staging_buffer,
      staging_memory
    );

    void* mapped = nullptr;
    check(vkMapMemory(device_, staging_memory, 0, image_size, 0, &mapped), "map texture staging memory");
    std::memcpy(mapped, image.rgba.data(), image.rgba.size());
    vkUnmapMemory(device_, staging_memory);

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = image.width;
    image_info.extent.height = image.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;

    TextureResource& texture = textures_[index];
    check(vkCreateImage(device_, &image_info, nullptr, &texture.image), "create sprite image");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, texture.image, &requirements);

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    check(vkAllocateMemory(device_, &allocate_info, nullptr, &texture.memory), "allocate sprite image memory");
    check(vkBindImageMemory(device_, texture.image, texture.memory, 0), "bind sprite image memory");

    transition_image_layout(texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging_buffer, texture.image, image.width, image.height);
    transition_image_layout(texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device_, staging_buffer, nullptr);
    vkFreeMemory(device_, staging_memory, nullptr);

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = texture.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    check(vkCreateImageView(device_, &view_info, nullptr, &texture.view), "create sprite image view");
  }

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;

  check(vkCreateSampler(device_, &sampler_info, nullptr, &texture_sampler_), "create sprite sampler");
}

void VulkanRenderer::create_texture_descriptor_sets() {
  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_size.descriptorCount = static_cast<std::uint32_t>(textures_.size());

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  pool_info.maxSets = static_cast<std::uint32_t>(textures_.size());

  check(vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_), "create sprite descriptor pool");

  std::vector<VkDescriptorSetLayout> layouts(textures_.size(), descriptor_set_layout_);
  VkDescriptorSetAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = descriptor_pool_;
  allocate_info.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
  allocate_info.pSetLayouts = layouts.data();

  std::vector<VkDescriptorSet> descriptor_sets(textures_.size());
  check(vkAllocateDescriptorSets(device_, &allocate_info, descriptor_sets.data()), "allocate sprite descriptor sets");

  for (std::size_t index = 0; index < textures_.size(); ++index) {
    textures_[index].descriptor_set = descriptor_sets[index];

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = textures_[index].view;
    image_info.sampler = texture_sampler_;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = textures_[index].descriptor_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  }
}

void VulkanRenderer::create_command_buffers() {
  command_buffers_.resize(framebuffers_.size());

  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = command_pool_;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = static_cast<std::uint32_t>(command_buffers_.size());

  check(vkAllocateCommandBuffers(device_, &allocate_info, command_buffers_.data()), "allocate command buffers");
}

void VulkanRenderer::record_command_buffer(std::uint32_t image_index, const SpriteFrame& frame) {
  VkCommandBuffer command_buffer = command_buffers_.at(image_index);
  check(vkResetCommandBuffer(command_buffer, 0), "reset command buffer");

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  check(vkBeginCommandBuffer(command_buffer, &begin_info), "begin command buffer");

  const RenderColor color = make_clear_color(frame.state);
  VkClearValue clear_color{};
  clear_color.color = {{color.r, color.g, color.b, color.a}};

  VkRenderPassBeginInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = render_pass_;
  render_pass_info.framebuffer = framebuffers_.at(image_index);
  render_pass_info.renderArea.offset = {0, 0};
  render_pass_info.renderArea.extent = swapchain_extent_;
  render_pass_info.clearValueCount = 1;
  render_pass_info.pClearValues = &clear_color;

  vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

  for (const SpriteDraw& sprite : frame.sprites) {
    const std::size_t texture_index = static_cast<std::size_t>(sprite.texture);
    if (texture_index >= textures_.size()) {
      continue;
    }

    const SpritePushConstants push_constants{
      .center_x = sprite.center_x_ndc,
      .center_y = sprite.center_y_ndc,
      .half_width = sprite.half_width_ndc,
      .half_height = sprite.half_height_ndc,
      .rotation_radians = sprite.rotation_radians,
      .tint_r = sprite.tint_r,
      .tint_g = sprite.tint_g,
      .tint_b = sprite.tint_b,
      .tint_a = sprite.tint_a,
    };
    vkCmdBindDescriptorSets(
      command_buffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline_layout_,
      0,
      1,
      &textures_[texture_index].descriptor_set,
      0,
      nullptr
    );
    vkCmdPushConstants(
      command_buffer,
      pipeline_layout_,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof(SpritePushConstants),
      &push_constants
    );
    vkCmdDraw(command_buffer, 6, 1, 0, 0);
  }

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, line_pipeline_);
  for (const LineDraw& line : frame.lines) {
    const LinePushConstants push_constants{
      .start_x = line.start_x_ndc,
      .start_y = line.start_y_ndc,
      .end_x = line.end_x_ndc,
      .end_y = line.end_y_ndc,
      .r = line.r,
      .g = line.g,
      .b = line.b,
      .a = line.a,
    };
    vkCmdPushConstants(
      command_buffer,
      line_pipeline_layout_,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof(LinePushConstants),
      &push_constants
    );
    vkCmdDraw(command_buffer, 2, 1, 0, 0);
  }

  vkCmdEndRenderPass(command_buffer);

  check(vkEndCommandBuffer(command_buffer), "end command buffer");
}

void VulkanRenderer::create_buffer(
  VkDeviceSize size,
  VkBufferUsageFlags usage,
  VkMemoryPropertyFlags properties,
  VkBuffer& buffer,
  VkDeviceMemory& memory
) {
  VkBufferCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.size = size;
  create_info.usage = usage;
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  check(vkCreateBuffer(device_, &create_info, nullptr, &buffer), "create buffer");

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, buffer, &requirements);

  VkMemoryAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate_info.allocationSize = requirements.size;
  allocate_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, properties);

  check(vkAllocateMemory(device_, &allocate_info, nullptr, &memory), "allocate buffer memory");
  check(vkBindBufferMemory(device_, buffer, memory, 0), "bind buffer memory");
}

VkCommandBuffer VulkanRenderer::begin_single_time_commands() {
  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandPool = command_pool_;
  allocate_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  check(vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer), "allocate one-shot command buffer");

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(command_buffer, &begin_info), "begin one-shot command buffer");

  return command_buffer;
}

void VulkanRenderer::end_single_time_commands(VkCommandBuffer command_buffer) {
  check(vkEndCommandBuffer(command_buffer), "end one-shot command buffer");

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  check(vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE), "submit one-shot command buffer");
  check(vkQueueWaitIdle(graphics_queue_), "wait one-shot command buffer");

  vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
}

void VulkanRenderer::transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
  (void)format;
  VkCommandBuffer command_buffer = begin_single_time_commands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::runtime_error("unsupported image layout transition");
  }

  vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  end_single_time_commands(command_buffer);
}

void VulkanRenderer::copy_buffer_to_image(VkBuffer buffer, VkImage image, std::uint32_t width, std::uint32_t height) {
  VkCommandBuffer command_buffer = begin_single_time_commands();

  VkBufferImageCopy region{};
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  end_single_time_commands(command_buffer);
}

void VulkanRenderer::create_sync_objects() {
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  check(vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_), "create image-available semaphore");
  check(vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_), "create render-finished semaphore");
  check(vkCreateFence(device_, &fence_info, nullptr, &in_flight_), "create in-flight fence");
}

std::uint32_t VulkanRenderer::find_graphics_present_queue_family(VkPhysicalDevice device) const {
  std::uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);
  std::vector<VkQueueFamilyProperties> families(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families.data());

  for (std::uint32_t index = 0; index < family_count; ++index) {
    VkBool32 present_support = VK_FALSE;
    check(vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface_, &present_support), "query surface support");

    if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && present_support == VK_TRUE) {
      return index;
    }
  }

  throw std::runtime_error("physical device has no graphics/present queue family");
}

std::uint32_t VulkanRenderer::find_memory_type(std::uint32_t type_filter, VkMemoryPropertyFlags properties) const {
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);

  for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
    const bool type_matches = (type_filter & (1U << index)) != 0;
    const bool properties_match = (memory_properties.memoryTypes[index].propertyFlags & properties) == properties;
    if (type_matches && properties_match) {
      return index;
    }
  }

  throw std::runtime_error("no suitable Vulkan memory type");
}

VkSurfaceFormatKHR VulkanRenderer::choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) const {
  const auto preferred = std::ranges::find_if(formats, [](const VkSurfaceFormatKHR& format) {
    return format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  });

  if (preferred != formats.end()) {
    return *preferred;
  }
  if (formats.empty()) {
    throw std::runtime_error("surface exposes no formats");
  }
  return formats.front();
}

VkPresentModeKHR VulkanRenderer::choose_present_mode(const std::vector<VkPresentModeKHR>& modes) const {
  if (std::ranges::find(modes, VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
    return VK_PRESENT_MODE_MAILBOX_KHR;
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::choose_extent(SDL_Window& window, const VkSurfaceCapabilitiesKHR& capabilities) const {
  if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSizeInPixels(&window, &width, &height)) {
    throw std::runtime_error(std::string{"SDL_GetWindowSizeInPixels failed: "} + SDL_GetError());
  }

  const auto clamped_width = std::clamp(
    static_cast<std::uint32_t>(width),
    capabilities.minImageExtent.width,
    capabilities.maxImageExtent.width
  );
  const auto clamped_height = std::clamp(
    static_cast<std::uint32_t>(height),
    capabilities.minImageExtent.height,
    capabilities.maxImageExtent.height
  );

  return VkExtent2D{clamped_width, clamped_height};
}

}  // namespace hyperverse
