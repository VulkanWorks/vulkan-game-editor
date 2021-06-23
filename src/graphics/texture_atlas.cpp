#include "texture_atlas.h"

#pragma warning(push, 0)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#pragma warning(pop)

#include <algorithm>
#include <iostream>

#include "../creature.h"
#include "../debug.h"
#include "../file.h"
#include "../logger.h"
#include "../position.h"
#include "compression.h"

namespace
{
    constexpr uint32_t SPRITE_SIZE = 32;

    // Signifies that the BMP is uncompressed.
    constexpr uint8_t BI_BITFIELDS = 0x03;

    // See https://en.wikipedia.org/wiki/BMP_file_format#Bitmap_file_header
    constexpr uint32_t OFFSET_OF_BMP_START_OFFSET = 10;
} // namespace

TextureAtlas::TextureAtlas(LZMACompressedBuffer &&buffer, uint32_t width, uint32_t height, uint32_t firstSpriteId, uint32_t lastSpriteId, SpriteLayout spriteLayout, std::filesystem::path sourceFile)
    : sourceFile(sourceFile), width(width), height(height), firstSpriteId(firstSpriteId), lastSpriteId(lastSpriteId), texture(std::move(buffer))
{
    switch (spriteLayout)
    {
        case SpriteLayout::ONE_BY_ONE:
            this->rows = 12;
            this->columns = 12;
            this->spriteWidth = SPRITE_SIZE;
            this->spriteHeight = SPRITE_SIZE;
            drawOffset.x = 0;
            drawOffset.y = 0;
            break;
        case SpriteLayout::ONE_BY_TWO:
            this->rows = 6;
            this->columns = 12;
            this->spriteWidth = SPRITE_SIZE;
            this->spriteHeight = SPRITE_SIZE * 2;
            drawOffset.x = 0;
            drawOffset.y = -1;
            break;
        case SpriteLayout::TWO_BY_ONE:
            this->rows = 12;
            this->columns = 6;
            this->spriteWidth = SPRITE_SIZE * 2;
            this->spriteHeight = SPRITE_SIZE;
            drawOffset.x = -1;
            drawOffset.y = 0;
            break;
        case SpriteLayout::TWO_BY_TWO:
            this->rows = 6;
            this->columns = 6;
            this->spriteWidth = SPRITE_SIZE * 2;
            this->spriteHeight = SPRITE_SIZE * 2;
            drawOffset.x = -1;
            drawOffset.y = -1;
            break;
        default:
            this->rows = 12;
            this->columns = 12;
            this->spriteWidth = SPRITE_SIZE;
            this->spriteHeight = SPRITE_SIZE;
            drawOffset.x = 0;
            drawOffset.y = 0;
            break;
    }
}

TextureAtlas::InternalTextureInfo TextureAtlas::internalTextureInfoNormalized(uint32_t spriteId) const
{
    DEBUG_ASSERT(firstSpriteId <= spriteId && spriteId <= lastSpriteId, "The TextureAtlas does not contain that sprite ID.");
    InternalTextureInfo info;

    uint32_t offset = spriteId - this->firstSpriteId;

    uint32_t row = offset / columns;
    uint32_t col = offset % columns;

    info.x = static_cast<float>(col) / columns;
    info.y = static_cast<float>(rows - row) / rows;

    info.width = static_cast<float>(spriteWidth) / this->width;
    info.height = static_cast<float>(spriteHeight) / this->height;

    return info;
}

const TextureWindow TextureAtlas::getTextureWindow(uint32_t spriteId, TextureInfo::CoordinateType coordinateType) const
{
    DEBUG_ASSERT(firstSpriteId <= spriteId && spriteId <= lastSpriteId, "The TextureAtlas does not contain that sprite ID.");

    if (coordinateType == TextureInfo::CoordinateType::Normalized)
    {
        auto info = internalTextureInfoNormalized(spriteId);

        return TextureWindow{info.x, info.y - info.height, info.x + info.width, info.y};
    }
    else
    {
        uint32_t offset = spriteId - this->firstSpriteId;

        auto row = offset / columns;
        auto col = offset % columns;

        const float x = static_cast<float>(col) * spriteWidth;
        const float y = static_cast<float>(rows - row - 1) * spriteHeight;

        const float width = static_cast<float>(spriteWidth);
        const float height = static_cast<float>(spriteHeight);

        return TextureWindow{x, y, width, height};
    }
}

const TextureWindow TextureAtlas::getTextureWindow(uint32_t spriteId, uint32_t variationId, TextureInfo::CoordinateType coordinateType) const
{
    // TODO Implement?
    ABORT_PROGRAM("Not implemented.");
}

void TextureAtlas::overlay(TextureAtlas *templateAtlas, uint32_t variationId, uint32_t templateSpriteId, uint32_t targetSpriteId, Outfit::Look look)
{
    DEBUG_ASSERT(templateAtlas->width == width, "Inconsistent atlas widths.");
    auto const [templateX, templateY] = templateAtlas->textureOffset(templateSpriteId);
    auto const [targetX, targetY] = textureOffset(targetSpriteId);

    auto atlasWidth = templateAtlas->width;

    const auto &templatePixels = templateAtlas->getOrCreateTexture().pixels();
    auto &targetPixels = getVariation(variationId)->texture.mutablePixelsTEST();

    auto lookPixel = [](uint8_t _look) -> Pixel {
        uint8_t r = (Creatures::TemplateOutfitLookupTable[_look] & 0xFF0000) >> 16;
        uint8_t g = (Creatures::TemplateOutfitLookupTable[_look] & 0xFF00) >> 8;
        uint8_t b = (Creatures::TemplateOutfitLookupTable[_look] & 0xFF);
        return {r, g, b, 255};
    };

    auto head = lookPixel(look.head());
    auto body = lookPixel(look.body());
    auto legs = lookPixel(look.legs());
    auto feet = lookPixel(look.feet());

    for (int y = targetY; y < targetY + spriteHeight; ++y)
    {
        int ty = templateY + y - targetY;
        for (int x = targetX; x < targetX + spriteWidth; ++x)
        {
            int tx = templateX + x - targetX;

            auto pixel = getPixelFromBMPTexture(tx, ty, atlasWidth, templatePixels);
            if (pixel == Pixels::Yellow)
            {
                multiplyPixelInBMP(x, y, atlasWidth, targetPixels, head);
            }
            else if (pixel == Pixels::Red)
            {
                multiplyPixelInBMP(x, y, atlasWidth, targetPixels, body);
            }
            else if (pixel == Pixels::Green)
            {
                multiplyPixelInBMP(x, y, atlasWidth, targetPixels, legs);
            }
            else if (pixel == Pixels::Blue)
            {
                multiplyPixelInBMP(x, y, atlasWidth, targetPixels, feet);
            }
            else if (pixel == Pixels::Magenta)
            {
                // Magenta
            }
            else
            {
                VME_LOG_D(std::format("{},{}: {},{},{},{}", tx, ty, pixel.r(), pixel.g(), pixel.b(), pixel.a()));
            }
        }
    }
}

TextureAtlasVariation *TextureAtlas::getVariation(uint32_t id)
{
    if (!variations)
    {
        variations = std::make_unique<std::vector<TextureAtlasVariation>>();
    }

    auto found = std::find_if(variations->begin(), variations->end(), [id](const auto &variation) {
        return variation.id == id;
    });

    if (found != variations->end())
    {
        return &(*found);
    }

    auto &variation = variations->emplace_back(id, getOrCreateTexture().deepCopy());
    return &variation;
}

std::pair<int, int> TextureAtlas::textureOffset(uint32_t spriteId)
{
    int spriteIndex = spriteId - firstSpriteId;

    auto topLeftX = spriteWidth * (spriteIndex % columns);
    auto topLeftY = spriteHeight * (spriteIndex / rows);

    return {topLeftX, topLeftY};
}

const TextureWindow TextureAtlas::getTextureWindowTopLeft(uint32_t spriteId) const
{
    auto info = internalTextureInfoNormalized(spriteId);

    return TextureWindow{info.x, info.y - info.height / 2, info.x + info.width / 2, info.y};
}

const std::pair<TextureWindow, TextureWindow> TextureAtlas::getTextureWindowTopLeftBottomRight(uint32_t spriteId) const
{
    auto info = internalTextureInfoNormalized(spriteId);

    return std::pair{
        TextureWindow{info.x, info.y - info.height / 2, info.x + info.width / 2, info.y},
        TextureWindow{info.x + info.width / 2, info.y - info.height, info.x + info.width, info.y - info.height / 2}};
}

const std::tuple<TextureWindow, TextureWindow, TextureWindow> TextureAtlas::getTextureWindowTopRightBottomRightBottomLeft(uint32_t spriteId) const
{
    auto info = internalTextureInfoNormalized(spriteId);

    return std::tuple{
        TextureWindow{info.x + info.width / 2, info.y - info.height / 2, info.x + info.width, info.y},
        TextureWindow{info.x + info.width / 2, info.y - info.height, info.x + info.width, info.y - info.height / 2},
        TextureWindow{info.x, info.y - info.height, info.x + info.width / 2, info.y - info.height / 2}};
}

void print_byte(uint8_t b)
{
    std::cout << std::hex << std::setw(2) << unsigned(b) << std::endl;
}

void nextN(std::vector<uint8_t> &buffer, uint32_t offset, uint32_t n)
{
    //std::cout << "Next " << std::to_string(n) << ":" << std::endl;
    uint8_t *data = buffer.data();
    for (uint32_t i = 0; i < n; ++i)
    {
        print_byte(*(data + offset + i));
    }

    std::cout << "---" << std::endl;
}

uint32_t readU32(std::vector<uint8_t> &buffer, uint32_t offset)
{
    uint32_t value;
    std::memcpy(&value, buffer.data() + offset, sizeof(uint32_t));
    return value;
}

void TextureAtlas::validateBmp(std::vector<uint8_t> &decompressed) const
{
    uint32_t width = readU32(decompressed, 0x12);
    if (width != TextureAtlasSize.width)
    {
        throw std::runtime_error("Texture atlas has incorrect width. Expected " + std::to_string(TextureAtlasSize.width) + " but received " + std::to_string(width) + ".");
    }

    uint32_t height = readU32(decompressed, 0x16);
    if (height != TextureAtlasSize.height)
    {
        throw std::runtime_error("Texture atlas has incorrect width. Expected " + std::to_string(TextureAtlasSize.height) + " but received " + std::to_string(height) + ".");
    }

    uint32_t compression = readU32(decompressed, 0x1E);
    if (compression != BI_BITFIELDS)
    {
        throw std::runtime_error("Texture atlas has incorrect compression. Expected BI_BITFIELDS but received " + std::to_string(compression) + ".");
    }
}

// void fixMagenta(std::vector<uint8_t> &buffer, uint32_t offset)
// {
//     for (auto cursor = 0; cursor < buffer.size() - 4 - offset; cursor += 4)
//     {
//         if (readU32(buffer, offset + cursor) == 0xFF00FF)
//         {
//             std::fill(buffer.begin() + offset + cursor, buffer.begin() + offset + cursor + 3, 0);
//         }
//     }
// }

Texture *TextureAtlas::getTexture()
{
    return std::holds_alternative<Texture>(texture) ? &std::get<Texture>(texture) : nullptr;
}

bool TextureAtlas::isCompressed() const
{
    return std::holds_alternative<LZMACompressedBuffer>(texture);
}

void TextureAtlas::decompressTexture() const
{
    DEBUG_ASSERT(std::holds_alternative<LZMACompressedBuffer>(texture), "Tried to decompress a TextureAtlas that does not contain CompressedBytes.");

    std::vector<uint8_t> decompressed = LZMA::decompress(std::move(std::get<LZMACompressedBuffer>(this->texture).buffer));
    validateBmp(decompressed);

    uint32_t offset;
    std::memcpy(&offset, decompressed.data() + OFFSET_OF_BMP_START_OFFSET, sizeof(uint32_t));

    texture = Texture(this->width, this->height, std::vector<uint8_t>(decompressed.begin() + offset, decompressed.end()));
}

Texture &TextureAtlas::getTexture(uint32_t variationId)
{
    if (!variations)
    {
        ABORT_PROGRAM("No variations.");
    }

    auto found = std::find_if(variations->begin(), variations->end(), [variationId](const auto &variation) {
        return variation.id == variationId;
    });

    if (found == variations->end())
    {
        ABORT_PROGRAM(std::format("The TextureAtlas did not have a Texture variation with id '{}'", variationId));
    }

    auto &result = (*found).texture;
    return result;
}

Texture &TextureAtlas::getOrCreateTexture() const
{
    if (std::holds_alternative<LZMACompressedBuffer>(texture))
    {
        decompressTexture();
    }

    return std::get<Texture>(this->texture);
}

Texture &TextureAtlas::getOrCreateTexture()
{
    return const_cast<const TextureAtlas *>(this)->getOrCreateTexture();
}

glm::vec4 TextureAtlas::getFragmentBounds(const TextureWindow window) const
{
    const float offsetX = 0.5f / this->width;
    const float offsetY = 0.5f / this->height;

    return {
        window.x0 + offsetX,
        window.y0 + offsetY,
        window.x1 - offsetX,
        window.y1 - offsetY,
    };
}

WorldPosition TextureAtlas::worldPosOffset() const noexcept
{
    return WorldPosition(drawOffset.x * SPRITE_SIZE, drawOffset.y * SPRITE_SIZE);
}

bool TextureAtlas::hasColorVariation(uint32_t variationId) const
{
    if (!variations)
        return false;

    auto found = std::find_if(variations->begin(), variations->end(), [variationId](const auto &variation) {
        return variation.id == variationId;
    });

    return found != variations->end();
}

TextureAtlasVariation::TextureAtlasVariation(uint32_t id, Texture texture)
    : id(id), texture(texture)
{
}
