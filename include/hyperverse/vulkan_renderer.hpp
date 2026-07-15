#pragma once

#include "hyperverse/render_frame.hpp"

#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace hyperverse {

class VulkanRenderer {
public:
  explicit VulkanRenderer(SDL_Window& window);
  ~VulkanRenderer();

  VulkanRenderer(const VulkanRenderer&) = delete;
  VulkanRenderer& operator=(const VulkanRenderer&) = delete;
  VulkanRenderer(VulkanRenderer&&) = delete;
  VulkanRenderer& operator=(VulkanRenderer&&) = delete;

  void draw_frame(const VulkanFrameSnapshot& frame = {});
  void wait_idle() const;

private:
  void create_instance(SDL_Window& window);
  void create_surface(SDL_Window& window);
  void pick_physical_device();
  void create_logical_device();
  void create_swapchain(SDL_Window& window);
  void create_image_views();
  void create_render_pass();
  void create_framebuffers();
  void create_command_pool();
  void create_command_buffers();
  void create_sync_objects();
  void record_command_buffer(std::uint32_t image_index, const VulkanFrameSnapshot& frame);

  [[nodiscard]] std::uint32_t find_graphics_present_queue_family(VkPhysicalDevice device) const;
  [[nodiscard]] VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) const;
  [[nodiscard]] VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes) const;
  [[nodiscard]] VkExtent2D choose_extent(SDL_Window& window, const VkSurfaceCapabilitiesKHR& capabilities) const;

  VkInstance instance_{VK_NULL_HANDLE};
  VkSurfaceKHR surface_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue graphics_queue_{VK_NULL_HANDLE};
  std::uint32_t graphics_queue_family_{0};

  VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  VkFormat swapchain_format_{VK_FORMAT_UNDEFINED};
  VkExtent2D swapchain_extent_{};
  std::vector<VkImage> swapchain_images_;
  std::vector<VkImageView> swapchain_image_views_;
  VkRenderPass render_pass_{VK_NULL_HANDLE};
  std::vector<VkFramebuffer> framebuffers_;
  VkCommandPool command_pool_{VK_NULL_HANDLE};
  std::vector<VkCommandBuffer> command_buffers_;
  VkSemaphore image_available_{VK_NULL_HANDLE};
  VkSemaphore render_finished_{VK_NULL_HANDLE};
  VkFence in_flight_{VK_NULL_HANDLE};
};

}  // namespace hyperverse
