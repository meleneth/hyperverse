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

void VulkanRenderer::create_texture_resources() {
  const std::vector<LoadedPng> images = load_sprite_textures();

  textures_.resize(images.size());
  for (std::size_t index = 0; index < images.size(); ++index) {
    const LoadedPng& image = images[index];
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



}  // namespace hyperverse
