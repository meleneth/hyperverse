#include "hyperverse/vulkan_renderer.hpp"

#include "vulkan_detail.hpp"

#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using namespace hyperverse::vulkan_detail;

namespace hyperverse {

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



void VulkanRenderer::recreate_swapchain() {
  if (window_ == nullptr) {
    throw std::runtime_error("renderer has no window for swapchain recreation");
  }

  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSizeInPixels(window_, &width, &height)) {
    throw std::runtime_error(std::string{"SDL_GetWindowSizeInPixels failed: "} + SDL_GetError());
  }
  if (width <= 0 || height <= 0) {
    return;
  }

  vkDeviceWaitIdle(device_);
  cleanup_swapchain();
  create_swapchain(*window_);
  create_image_views();
  create_render_pass();
  create_graphics_pipeline();
  create_line_pipeline();
  create_framebuffers();
  create_command_buffers();
}



void VulkanRenderer::cleanup_swapchain() {
  if (!command_buffers_.empty()) {
    vkFreeCommandBuffers(device_, command_pool_, static_cast<std::uint32_t>(command_buffers_.size()), command_buffers_.data());
    command_buffers_.clear();
  }
  for (VkFramebuffer framebuffer : framebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }
  framebuffers_.clear();
  if (graphics_pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, graphics_pipeline_, nullptr);
    graphics_pipeline_ = VK_NULL_HANDLE;
  }
  if (line_pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, line_pipeline_, nullptr);
    line_pipeline_ = VK_NULL_HANDLE;
  }
  if (pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    pipeline_layout_ = VK_NULL_HANDLE;
  }
  if (line_pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, line_pipeline_layout_, nullptr);
    line_pipeline_layout_ = VK_NULL_HANDLE;
  }
  if (render_pass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, render_pass_, nullptr);
    render_pass_ = VK_NULL_HANDLE;
  }
  for (VkImageView image_view : swapchain_image_views_) {
    vkDestroyImageView(device_, image_view, nullptr);
  }
  swapchain_image_views_.clear();
  swapchain_images_.clear();
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
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
