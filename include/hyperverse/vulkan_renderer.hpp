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
  void draw_frame(const SpriteFrame& frame);
  void wait_idle() const;
  [[nodiscard]] std::uint32_t width() const;
  [[nodiscard]] std::uint32_t height() const;

private:
  void create_instance(SDL_Window& window);
  void create_surface(SDL_Window& window);
  void pick_physical_device();
  void create_logical_device();
  void create_swapchain(SDL_Window& window);
  void recreate_swapchain();
  void cleanup_swapchain();
  void create_image_views();
  void create_render_pass();
  void create_descriptor_set_layout();
  void create_graphics_pipeline();
  void create_line_pipeline();
  void create_framebuffers();
  void create_command_pool();
  void create_texture_resources();
  void create_texture_descriptor_sets();
  void create_command_buffers();
  void create_sync_objects();
  void record_command_buffer(std::uint32_t image_index, const SpriteFrame& frame);
  void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
  void copy_buffer_to_image(VkBuffer buffer, VkImage image, std::uint32_t width, std::uint32_t height);
  void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
  [[nodiscard]] VkCommandBuffer begin_single_time_commands();
  void end_single_time_commands(VkCommandBuffer command_buffer);

  [[nodiscard]] std::uint32_t find_graphics_present_queue_family(VkPhysicalDevice device) const;
  [[nodiscard]] std::uint32_t find_memory_type(std::uint32_t type_filter, VkMemoryPropertyFlags properties) const;
  [[nodiscard]] VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) const;
  [[nodiscard]] VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes) const;
  [[nodiscard]] VkExtent2D choose_extent(SDL_Window& window, const VkSurfaceCapabilitiesKHR& capabilities) const;

  VkInstance instance_{VK_NULL_HANDLE};
  SDL_Window* window_{nullptr};
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
  VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
  VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
  VkPipeline graphics_pipeline_{VK_NULL_HANDLE};
  VkPipelineLayout line_pipeline_layout_{VK_NULL_HANDLE};
  VkPipeline line_pipeline_{VK_NULL_HANDLE};
  std::vector<VkFramebuffer> framebuffers_;
  VkCommandPool command_pool_{VK_NULL_HANDLE};
  std::vector<VkCommandBuffer> command_buffers_;
  VkSemaphore image_available_{VK_NULL_HANDLE};
  VkSemaphore render_finished_{VK_NULL_HANDLE};
  VkFence in_flight_{VK_NULL_HANDLE};

  struct TextureResource {
    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
  };

  VkSampler texture_sampler_{VK_NULL_HANDLE};
  VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
  std::vector<TextureResource> textures_;
};

}  // namespace hyperverse
