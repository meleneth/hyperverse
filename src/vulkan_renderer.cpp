#include "hyperverse/vulkan_renderer.hpp"

#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void check(VkResult result, const char* message) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(message);
  }
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
  create_framebuffers();
  create_command_pool();
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

void VulkanRenderer::create_command_buffers() {
  command_buffers_.resize(framebuffers_.size());

  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = command_pool_;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = static_cast<std::uint32_t>(command_buffers_.size());

  check(vkAllocateCommandBuffers(device_, &allocate_info, command_buffers_.data()), "allocate command buffers");
}

void VulkanRenderer::record_command_buffer(std::uint32_t image_index, const VulkanFrameSnapshot& frame) {
  VkCommandBuffer command_buffer = command_buffers_.at(image_index);
  check(vkResetCommandBuffer(command_buffer, 0), "reset command buffer");

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  check(vkBeginCommandBuffer(command_buffer, &begin_info), "begin command buffer");

  const RenderColor color = make_clear_color(frame);
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
  vkCmdEndRenderPass(command_buffer);

  check(vkEndCommandBuffer(command_buffer), "end command buffer");
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
