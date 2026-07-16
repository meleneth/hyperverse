#include "hyperverse/vulkan_renderer.hpp"

#include "vulkan_detail.hpp"

#include <SDL3/SDL_vulkan.h>

#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace hyperverse::vulkan_detail;

namespace hyperverse {

VulkanRenderer::VulkanRenderer(SDL_Window& window) {
  window_ = &window;
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
  cleanup_swapchain();
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

  std::uint32_t image_index = 0;
  VkResult acquire_result =
    vkAcquireNextImageKHR(device_, swapchain_, std::numeric_limits<std::uint64_t>::max(), image_available_, VK_NULL_HANDLE, &image_index);
  if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain();
    return;
  }
  if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swapchain image");
  }

  check(vkResetFences(device_, 1, &in_flight_), "reset fence");
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
  if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
    recreate_swapchain();
    return;
  }
  if (present_result != VK_SUCCESS) {
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

void VulkanRenderer::create_command_pool() {
  VkCommandPoolCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  create_info.queueFamilyIndex = graphics_queue_family_;

  check(vkCreateCommandPool(device_, &create_info, nullptr, &command_pool_), "create command pool");
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

}  // namespace hyperverse
