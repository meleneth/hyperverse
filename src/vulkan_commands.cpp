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
      .padding0 = static_cast<float>(swapchain_extent_.width) / static_cast<float>(swapchain_extent_.height),
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



}  // namespace hyperverse
