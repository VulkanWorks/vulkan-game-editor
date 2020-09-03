#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cmath>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <ostream>
#include <unordered_map>

#include "vulkan_helpers.h"

#include "buffer.h"
#include "../util.h"

// QVulkanWindow works correctly with UNORM. If not using Vulkan, VK_FORMAT_B8G8R8A8_SRGB works.
constexpr VkFormat ColorFormat = VK_FORMAT_B8G8R8A8_UNORM;

std::unordered_map<SolidColor, std::unique_ptr<Texture>> solidColorTextures;

Texture::Texture(uint32_t width, uint32_t height, std::vector<uint8_t> pixels, Texture::Descriptor descriptor)
{
  init(width, height, pixels.data(), descriptor);
}

Texture::Texture(uint32_t width, uint32_t height, uint8_t *pixels, Texture::Descriptor descriptor)
{
  init(width, height, pixels, descriptor);
}

Texture::Texture(const std::string &filename, Texture::Descriptor descriptor)
{

  int width, height, channels;

  stbi_uc *pixels = stbi_load(filename.c_str(),
                              &width,
                              &height,
                              &channels,
                              STBI_rgb_alpha);

  // mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

  if (!pixels)
  {
    throw std::runtime_error("failed to load texture image!");
  }

  init(static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixels, descriptor);

  stbi_image_free(pixels);
}

void Texture::init(uint32_t width, uint32_t height, uint8_t *pixels, Texture::Descriptor descriptor)
{
  this->layout = VK_NULL_HANDLE;
  this->width = width;
  this->height = height;
  imageSize = static_cast<uint64_t>(width) * height * 4;

  VkDevice device = g_window->device();
  std::vector<uint8_t> vectorBuffer(pixels, pixels + width * height * 4);
  this->pixels = vectorBuffer;

  initVulkanResources(descriptor);
}

void Texture::initVulkanResources(Texture::Descriptor descriptor)
{
  auto stagingBuffer = Buffer::create(imageSize,
                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  Buffer::copyToMemory(stagingBuffer.deviceMemory, pixels.data(), imageSize);

  createImage(width,
              height,
              mipLevels,
              ColorFormat,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
              textureImage,
              textureImageMemory);

  VulkanHelpers::transitionImageLayout(textureImage,
                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VulkanHelpers::copyBufferToImage(stagingBuffer.buffer,
                                   textureImage,
                                   static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height));

  VulkanHelpers::transitionImageLayout(textureImage,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  descriptorSet = createDescriptorSet(descriptor);
  initialized = true;
}

void Texture::releaseVulkanResources()
{
  VkDevice device = g_window->device();
  g_vk->vkDestroyImage(device, textureImage, nullptr);
  g_vk->vkFreeMemory(device, textureImageMemory, nullptr);

  initialized = false;
}

// static
void Texture::releaseSolidColorTextures()
{
  for (auto &[_, texture] : solidColorTextures)
  {
    texture->releaseVulkanResources();
  }
}

void Texture::createImage(uint32_t width,
                          uint32_t height,
                          uint32_t mipLevels,
                          VkFormat format,
                          VkImageTiling tiling,
                          VkImageUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          VkImage &image,
                          VkDeviceMemory &imageMemory)
{
  VkDevice device = g_window->device();
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (g_vk->vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
  {
    throw std::runtime_error("failed to create image!");
  }

  VkMemoryRequirements memRequirements;
  g_vk->vkGetImageMemoryRequirements(device, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = VulkanHelpers::findMemoryType(g_window->physicalDevice(), memRequirements.memoryTypeBits, properties);

  if (g_vk->vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
  {
    throw std::runtime_error("failed to allocate image memory!");
  }

  g_vk->vkBindImageMemory(device, image, imageMemory, 0);
}

VkSampler Texture::createSampler()
{
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;

  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  // samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  // samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  // samplerInfo.maxLod = static_cast<float>(mipLevels);

  VkSampler sampler;
  if (g_vk->vkCreateSampler(g_window->device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create texture sampler!");
  }

  return sampler;
}

void Texture::update(Texture::Descriptor descriptor)
{
  if (!initialized)
  {
    initVulkanResources(descriptor);
  }
}

VkDescriptorSet Texture::createDescriptorSet(Texture::Descriptor descriptor)
{
  VkImageView imageView = VulkanHelpers::createImageView(g_window->device(), textureImage, ColorFormat);
  VkSampler sampler = createSampler();

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptor.pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &descriptor.layout;

  if (g_vk->vkAllocateDescriptorSets(g_window->device(), &allocInfo, &descriptorSet) != VK_SUCCESS)
  {
    throw std::runtime_error("failed to allocate texture descriptor set");
  }

  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = imageView;
  imageInfo.sampler = sampler;

  VkWriteDescriptorSet descriptorWrites = {};
  descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites.dstSet = descriptorSet;
  descriptorWrites.dstBinding = 0;
  descriptorWrites.dstArrayElement = 0;
  descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrites.descriptorCount = 1;
  descriptorWrites.pImageInfo = &imageInfo;

  g_vk->vkUpdateDescriptorSets(g_window->device(), 1, &descriptorWrites, 0, nullptr);

  return descriptorSet;
}

int Texture::getWidth()
{
  return width;
}

int Texture::getHeight()
{
  return height;
}

VkDescriptorSet Texture::getDescriptorSet()
{
  return descriptorSet;
}

TextureWindow Texture::getTextureWindow()
{
  return TextureWindow{0.0f, 0.0f, 1.0f, 1.0f};
}

uint32_t asArgb(SolidColor color)
{
  return to_underlying(color);
}

Texture &Texture::getOrCreateSolidTexture(SolidColor color, Texture::Descriptor descriptor)
{
  Texture *texture = getSolidTexture(color);
  if (texture)
  {
    return *texture;
  }
  else
  {
    std::vector<uint32_t> buffer(32 * 32);
    std::fill(buffer.begin(), buffer.end(), asArgb(color));

    solidColorTextures.emplace(color, std::make_unique<Texture>(32, 32, (uint8_t *)buffer.data(), descriptor));

    return *solidColorTextures.at(color).get();
  }
}

Texture *Texture::getSolidTexture(SolidColor color)
{
  auto found = solidColorTextures.find(color);
  return found != solidColorTextures.end() ? found->second.get() : nullptr;
}
