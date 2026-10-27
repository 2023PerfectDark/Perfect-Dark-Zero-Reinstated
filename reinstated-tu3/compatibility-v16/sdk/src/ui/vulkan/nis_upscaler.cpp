/**
 * @file        ui/vulkan/nis_upscaler.cpp
 * @brief       NVIDIA Image Scaling (NIS) compute pass for the Vulkan presenter
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from rexglue-sdk PR #527.
 *              NIS itself: thirdparty/nis (NVIDIA, MIT License).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include "nis_upscaler.h"

#include <cstddef>
#include <cstring>
#include <iterator>

#include <NIS_Config.h>

#include <rex/logging.h>
#include <rex/ui/gpu_completion_timeline.h>
#include <rex/ui/vulkan/device.h>
#include <rex/ui/vulkan/util.h>

namespace rex::ui::vulkan {

namespace shaders {
#include "../shaders/vulkan_spirv/guest_output_nis_scaler_cs.h"
}

namespace {

constexpr VkFormat kCoefficientFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr uint32_t kBlockWidth = 32;
constexpr uint32_t kBlockHeight = 24;
constexpr uint32_t kPushConstantSize = uint32_t(offsetof(NISConfig, reserved1) + sizeof(float));
constexpr VkDeviceSize kCoefficientBytes = sizeof(coef_scale);

static_assert(sizeof(coef_scale) == sizeof(coef_usm));
static_assert(kPushConstantSize <= 128);

}  // namespace

std::unique_ptr<VulkanNisUpscaler> VulkanNisUpscaler::Create(const VulkanDevice* vulkan_device,
                                                             VkSampler linear_clamp_sampler) {
  auto upscaler = std::unique_ptr<VulkanNisUpscaler>(new VulkanNisUpscaler(vulkan_device));
  if (!upscaler->Initialize(linear_clamp_sampler)) {
    return nullptr;
  }
  return upscaler;
}

VulkanNisUpscaler::~VulkanNisUpscaler() {
  const VulkanDevice::Functions& dfn = vulkan_device_->functions();
  const VkDevice device = vulkan_device_->device();
  util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device, staging_buffer_);
  util::DestroyAndNullHandle(dfn.vkFreeMemory, device, staging_memory_);
  for (Coefficients* coefficients : {&scaler_, &usm_}) {
    util::DestroyAndNullHandle(dfn.vkDestroyImageView, device, coefficients->view);
    util::DestroyAndNullHandle(dfn.vkDestroyImage, device, coefficients->image);
    util::DestroyAndNullHandle(dfn.vkFreeMemory, device, coefficients->memory);
  }
  util::DestroyAndNullHandle(dfn.vkDestroyDescriptorPool, device, descriptor_pool_);
  util::DestroyAndNullHandle(dfn.vkDestroyPipeline, device, pipeline_);
  util::DestroyAndNullHandle(dfn.vkDestroyPipelineLayout, device, pipeline_layout_);
  util::DestroyAndNullHandle(dfn.vkDestroyDescriptorSetLayout, device, descriptor_set_layout_);
}

bool VulkanNisUpscaler::Initialize(VkSampler linear_clamp_sampler) {
  const VulkanDevice::Functions& dfn = vulkan_device_->functions();
  const VkDevice device = vulkan_device_->device();
  sampler_ = linear_clamp_sampler;

  // The output is the guest output format (A2B10G10R10), an extended storage
  // image format.
  if (!vulkan_device_->properties().shaderStorageImageExtendedFormats) {
    REXLOG_WARN("VulkanNisUpscaler: shaderStorageImageExtendedFormats is not supported");
    return false;
  }

  std::array<VkDescriptorSetLayoutBinding, 5> bindings = {};
  const VkDescriptorType binding_types[] = {
      VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE};
  for (uint32_t i = 0; i < bindings.size(); ++i) {
    bindings[i].binding = i;
    bindings[i].descriptorType = binding_types[i];
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  }
  bindings[0].pImmutableSamplers = &sampler_;

  VkDescriptorSetLayoutCreateInfo set_layout_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  set_layout_info.bindingCount = uint32_t(bindings.size());
  set_layout_info.pBindings = bindings.data();
  if (dfn.vkCreateDescriptorSetLayout(device, &set_layout_info, nullptr, &descriptor_set_layout_) !=
      VK_SUCCESS) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to create the descriptor set layout");
    return false;
  }

  VkPushConstantRange push_constant_range = {};
  push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  push_constant_range.size = kPushConstantSize;
  VkPipelineLayoutCreateInfo pipeline_layout_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
  pipeline_layout_info.pushConstantRangeCount = 1;
  pipeline_layout_info.pPushConstantRanges = &push_constant_range;
  if (dfn.vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout_) !=
      VK_SUCCESS) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to create the pipeline layout");
    return false;
  }

  VkShaderModule shader =
      util::CreateShaderModule(vulkan_device_, shaders::guest_output_nis_scaler_cs,
                               sizeof(shaders::guest_output_nis_scaler_cs));
  if (shader == VK_NULL_HANDLE) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to create the shader module");
    return false;
  }
  VkComputePipelineCreateInfo pipeline_info = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipeline_info.stage.module = shader;
  pipeline_info.stage.pName = "main";
  pipeline_info.layout = pipeline_layout_;
  const VkResult pipeline_result =
      dfn.vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_);
  dfn.vkDestroyShaderModule(device, shader, nullptr);
  if (pipeline_result != VK_SUCCESS) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to create the compute pipeline");
    return false;
  }

  const VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, kDescriptorSetCount},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kDescriptorSetCount * 3},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kDescriptorSetCount},
  };
  VkDescriptorPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pool_info.maxSets = kDescriptorSetCount;
  pool_info.poolSizeCount = uint32_t(std::size(pool_sizes));
  pool_info.pPoolSizes = pool_sizes;
  if (dfn.vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to create the descriptor pool");
    return false;
  }
  std::array<VkDescriptorSetLayout, kDescriptorSetCount> set_layouts;
  set_layouts.fill(descriptor_set_layout_);
  VkDescriptorSetAllocateInfo set_allocate_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  set_allocate_info.descriptorPool = descriptor_pool_;
  set_allocate_info.descriptorSetCount = kDescriptorSetCount;
  set_allocate_info.pSetLayouts = set_layouts.data();
  if (dfn.vkAllocateDescriptorSets(device, &set_allocate_info, descriptor_sets_.data()) !=
      VK_SUCCESS) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to allocate the descriptor sets");
    return false;
  }

  if (!CreateCoefficients(scaler_) || !CreateCoefficients(usm_)) {
    return false;
  }

  uint32_t staging_memory_type = 0;
  VkDeviceSize staging_memory_size = 0;
  if (!util::CreateDedicatedAllocationBuffer(
          vulkan_device_, kCoefficientBytes * 2, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          util::MemoryPurpose::kUpload, staging_buffer_, staging_memory_, &staging_memory_type,
          &staging_memory_size)) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to create the coefficient staging buffer");
    return false;
  }
  void* mapping = nullptr;
  if (dfn.vkMapMemory(device, staging_memory_, 0, VK_WHOLE_SIZE, 0, &mapping) != VK_SUCCESS) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to map the coefficient staging buffer");
    return false;
  }
  std::memcpy(mapping, coef_scale, kCoefficientBytes);
  std::memcpy(static_cast<uint8_t*>(mapping) + kCoefficientBytes, coef_usm, kCoefficientBytes);
  util::FlushMappedMemoryRange(vulkan_device_, staging_memory_, staging_memory_type, 0,
                               staging_memory_size, VK_WHOLE_SIZE);
  dfn.vkUnmapMemory(device, staging_memory_);
  return true;
}

bool VulkanNisUpscaler::CreateCoefficients(Coefficients& coefficients) {
  const VulkanDevice::Functions& dfn = vulkan_device_->functions();
  const VkDevice device = vulkan_device_->device();

  VkImageCreateInfo image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = kCoefficientFormat;
  image_info.extent = {kCoefficientWidth, kCoefficientHeight, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (!util::CreateDedicatedAllocationImage(vulkan_device_, image_info,
                                            util::MemoryPurpose::kDeviceLocal, coefficients.image,
                                            coefficients.memory)) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to create a coefficient image");
    return false;
  }

  VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  view_info.image = coefficients.image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = kCoefficientFormat;
  view_info.subresourceRange = util::InitializeSubresourceRange();
  if (dfn.vkCreateImageView(device, &view_info, nullptr, &coefficients.view) != VK_SUCCESS) {
    REXLOG_ERROR("VulkanNisUpscaler: Failed to create a coefficient image view");
    return false;
  }
  return true;
}

void VulkanNisUpscaler::UploadCoefficients(VkCommandBuffer command_buffer) {
  const VulkanDevice::Functions& dfn = vulkan_device_->functions();
  const VkImage images[] = {scaler_.image, usm_.image};

  std::array<VkImageMemoryBarrier, 2> barriers = {};
  for (size_t i = 0; i < barriers.size(); ++i) {
    barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[i].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[i].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[i].image = images[i];
    barriers[i].subresourceRange = util::InitializeSubresourceRange();
  }
  dfn.vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                           uint32_t(barriers.size()), barriers.data());

  for (size_t i = 0; i < barriers.size(); ++i) {
    VkBufferImageCopy copy = {};
    copy.bufferOffset = kCoefficientBytes * i;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {kCoefficientWidth, kCoefficientHeight, 1};
    dfn.vkCmdCopyBufferToImage(command_buffer, staging_buffer_, images[i],
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  }

  for (VkImageMemoryBarrier& barrier : barriers) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }
  dfn.vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                           uint32_t(barriers.size()), barriers.data());
  coefficients_uploaded_ = true;
}

bool VulkanNisUpscaler::Dispatch(VkCommandBuffer command_buffer,
                                 GPUCompletionTimeline& completion_timeline, uint64_t submission,
                                 VkImageView input_view, uint32_t input_width,
                                 uint32_t input_height, VkImageView output_view,
                                 uint32_t output_width, uint32_t output_height, float sharpness) {
  NISConfig config = {};
  if (!NVScalerUpdateConfig(config, sharpness, 0, 0, input_width, input_height, input_width,
                            input_height, 0, 0, output_width, output_height, output_width,
                            output_height)) {
    return false;
  }

  const VulkanDevice::Functions& dfn = vulkan_device_->functions();
  const VkDevice device = vulkan_device_->device();

  if (!coefficients_uploaded_) {
    UploadCoefficients(command_buffer);
  }

  // A descriptor set per submission in a small ring: wait for the submission
  // that used this one last before rewriting it.
  const size_t set_index = size_t(submission % kDescriptorSetCount);
  if (descriptor_set_submissions_[set_index] != submission) {
    completion_timeline.AwaitSubmissionAndUpdateCompleted(descriptor_set_submissions_[set_index]);
  }
  descriptor_set_submissions_[set_index] = submission;
  const VkDescriptorSet descriptor_set = descriptor_sets_[set_index];

  const VkDescriptorImageInfo image_infos[] = {
      {VK_NULL_HANDLE, input_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      {VK_NULL_HANDLE, output_view, VK_IMAGE_LAYOUT_GENERAL},
      {VK_NULL_HANDLE, scaler_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      {VK_NULL_HANDLE, usm_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
  };
  const VkDescriptorType image_types[] = {
      VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE};
  std::array<VkWriteDescriptorSet, 4> writes = {};
  for (uint32_t i = 0; i < writes.size(); ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = descriptor_set;
    writes[i].dstBinding = i + 1;
    writes[i].descriptorCount = 1;
    writes[i].descriptorType = image_types[i];
    writes[i].pImageInfo = &image_infos[i];
  }
  dfn.vkUpdateDescriptorSets(device, uint32_t(writes.size()), writes.data(), 0, nullptr);

  dfn.vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
  dfn.vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0,
                              1, &descriptor_set, 0, nullptr);
  dfn.vkCmdPushConstants(command_buffer, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         kPushConstantSize, &config);
  dfn.vkCmdDispatch(command_buffer, (output_width + kBlockWidth - 1) / kBlockWidth,
                    (output_height + kBlockHeight - 1) / kBlockHeight, 1);
  if (logged_input_width_ != input_width || logged_input_height_ != input_height ||
      logged_output_width_ != output_width || logged_output_height_ != output_height) {
    logged_input_width_ = input_width;
    logged_input_height_ = input_height;
    logged_output_width_ = output_width;
    logged_output_height_ = output_height;
    REXLOG_INFO("VulkanNisUpscaler: {}x{} to {}x{}, sharpness {:.2f}", input_width, input_height,
                output_width, output_height, sharpness);
  }
  return true;
}

}  // namespace rex::ui::vulkan
