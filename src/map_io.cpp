#include "map_io.h"

#include <iostream>
#include <optional>
#include <string>

#include "debug.h"
#include "definitions.h"
#include "tile.h"
#include "version.h"

#pragma warning(push)
#pragma warning(disable : 26812)

using namespace OTBM;

namespace
{
  enum class Token
  {
    Start = 0xFE,
    End = 0xFF,
    Escape = 0xFD,
  };
}

inline bool operator==(const uint8_t lhs, const Token &rhs)
{
  return lhs == to_underlying(rhs);
}

constexpr uint32_t DEFAULT_BUFFER_SIZE = 0xFFFF;

SaveBuffer::SaveBuffer(std::ofstream &stream)
    : stream(stream), maxBufferSize(DEFAULT_BUFFER_SIZE)
{
  buffer.reserve(DEFAULT_BUFFER_SIZE);
}

void SaveBuffer::writeBytes(const uint8_t *cursor, size_t amount)
{
  while (amount > 0)
  {
    if (*cursor == Token::Start || *cursor == Token::End || *cursor == Token::Escape)
    {
      if (buffer.size() + 1 >= maxBufferSize)
      {
        flushToFile();
      }
      buffer.emplace_back(Token::Escape);
      std::cout << std::hex << static_cast<int>(buffer.back()) << std::endl;
    }

    if (buffer.size() + 1 >= maxBufferSize)
    {
      flushToFile();
    }
    buffer.emplace_back(*cursor);
    std::cout << std::hex << static_cast<int>(buffer.back()) << std::endl;

    ++cursor;
    --amount;
  }
}

void SaveBuffer::startNode(Node_t nodeType)
{
  if (buffer.size() + 2 >= maxBufferSize)
  {
    flushToFile();
  }

  buffer.emplace_back(Token::Start);
  std::cout << std::hex << static_cast<int>(Token::Start) << std::endl;

  buffer.emplace_back(nodeType);
  std::cout << std::hex << static_cast<int>(nodeType) << std::endl;
}

void SaveBuffer::endNode()
{
  if (buffer.size() + 1 >= maxBufferSize)
  {
    flushToFile();
  }

  buffer.emplace_back(Token::End);
  std::cout << std::hex << static_cast<int>(Token::End) << std::endl;
}

void SaveBuffer::writeU8(uint8_t value)
{
  writeBytes(&value, 1);
}

void SaveBuffer::writeU16(uint16_t value)
{
  writeBytes(reinterpret_cast<uint8_t *>(&value), 2);
}

void SaveBuffer::writeU32(uint32_t value)
{
  writeBytes(reinterpret_cast<uint8_t *>(&value), 4);
}

void SaveBuffer::writeU64(uint64_t value)
{
  writeBytes(reinterpret_cast<uint8_t *>(&value), 8);
}

void SaveBuffer::writeString(const std::string &s)
{
  if (s.size() > UINT16_MAX)
  {
    ABORT_PROGRAM("OTBM does not support strings larger than 65535 bytes.");
  }

  writeU16(static_cast<uint16_t>(s.size()));
  writeBytes(reinterpret_cast<uint8_t *>(const_cast<char *>(s.data())), s.size());
}

void SaveBuffer::writeRawString(const std::string &s)
{
  writeBytes(reinterpret_cast<uint8_t *>(const_cast<char *>(s.data())), s.size());
}

void SaveBuffer::writeLongString(const std::string &s)
{
  if (s.size() > UINT32_MAX)
  {
    ABORT_PROGRAM("OTBM does not support long strings larger than 2^32 bytes.");
  }

  writeU32(static_cast<uint32_t>(s.size()));
  writeBytes(reinterpret_cast<uint8_t *>(const_cast<char *>(s.data())), s.size());
}

void SaveBuffer::flushToFile()
{
  std::cout << "flushToFile()" << std::endl;
  stream.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
  buffer.clear();
}

void SaveBuffer::finish()
{
  flushToFile();
}

void MapIO::saveMap(Map &map)
{
  std::ofstream stream;
  SaveBuffer buffer = SaveBuffer(stream);

  stream.open("map2.otbm", std::ofstream::out | std::ios::binary | std::ofstream::trunc);

  buffer.writeRawString("OTBM");

  buffer.startNode(Node_t::Root);
  {
    OTBMVersion otbmVersion = map.getMapVersion().otbmVersion;
    buffer.writeU32(static_cast<uint32_t>(otbmVersion));

    buffer.writeU16(map.width());
    buffer.writeU16(map.height());

    buffer.writeU32(Items::items.getOtbVersionInfo().majorVersion);
    buffer.writeU32(Items::items.getOtbVersionInfo().minorVersion);

    buffer.startNode(Node_t::MapData);
    {
      buffer.writeU8(NodeAttribute::Description);
      buffer.writeString("Saved by VME (Vulkan Map Editor)" + __VME_VERSION__);

      buffer.writeU8(NodeAttribute::Description);
      buffer.writeString(map.getDescription());

      buffer.writeU8(NodeAttribute::ExternalSpawnFile);
      buffer.writeString("map.spawn.xml");

      buffer.writeU8(NodeAttribute::ExternalHouseFile);
      buffer.writeString("map.house.xml");

      // Tiles
      uint32_t savedTiles = 0;

      int x = -1;
      int y = -1;
      int z = -1;

      bool emptyMap = true;

      Serializer serializer(buffer, map.getMapVersion());

      for (const auto &location : map.begin())
      {
        ++savedTiles;

        Tile *tile = location->tile();

        // We can skip the tile if it has no entities
        if (!tile || tile->getEntityCount() == 0)
        {
          continue;
        }

        const Position &pos = location->position();

        // Need new node?
        if (pos.x < x || pos.x > x + 0xFF || pos.y < y || pos.y > y + 0xFF || pos.z != z)
        {
          if (!emptyMap)
          {
            buffer.endNode();
          }
          emptyMap = false;

          buffer.startNode(Node_t::TileArea);

          x = pos.x & 0xFF00;
          buffer.writeU16(x);

          y = pos.y & 0xFF00;
          buffer.writeU16(y);

          z = pos.z;
          buffer.writeU8(z);
        }

        bool isHouseTile = false;
        buffer.startNode(isHouseTile ? Node_t::Housetile : Node_t::Tile);

        buffer.writeU8(location->x() & 0xFF);
        buffer.writeU8(location->y() & 0xFF);

        if (isHouseTile)
        {
          uint32_t houseId = 0;
          buffer.writeU32(houseId);
        }

        if (tile->mapFlags())
        {
          buffer.writeU8(NodeAttribute::TileFlags);
          buffer.writeU32(tile->mapFlags());
        }

        Item *ground = tile->ground();
        if (ground)
        {
          if (ground->hasAttributes())
          {
            serializer.serializeItem(*ground);
          }
          else
          {
            buffer.writeU16(ground->serverId());
          }
        }

        for (const Item &item : tile->items())
        {
          serializer.serializeItem(item);
        }

        buffer.endNode();
      }

      // Close the last node
      if (!emptyMap)
      {
        buffer.endNode();
      }

      buffer.startNode(Node_t::Towns);
      for (auto &townEntry : map.towns())
      {
        const Town &town = townEntry.second;
        const Position &townPos = town.getTemplePosition();
        buffer.startNode(Node_t::Town);

        buffer.writeU32(town.getID());
        buffer.writeString(town.getName());
        buffer.writeU16(townPos.x);
        buffer.writeU16(townPos.y);
        buffer.writeU8(townPos.z);

        buffer.endNode();
      }
      buffer.endNode();

      if (otbmVersion >= OTBMVersion::MAP_OTBM_3)
      {
        // TODO write waypoints
        // TODO See RME: iomap_otb.cpp line 1415
      }
    }

    buffer.endNode();
  }
  buffer.endNode();

  buffer.finish();

  stream.close();
}

void MapIO::Serializer::serializeItem(const Item &item)
{
  buffer.startNode(Node_t::Item);
  buffer.writeU16(item.serverId());

  serializeItemAttributes(item);

  buffer.endNode();
}

void MapIO::Serializer::serializeItemAttributes(const Item &item)
{
  if (mapVersion.otbmVersion >= OTBMVersion::MAP_OTBM_2)
  {
    const ItemType &itemType = *item.itemType;
    if (itemType.usesSubType())
    {
      buffer.writeU8(NodeAttribute::Count);
      buffer.writeU8(item.getSubtype());
    }
  }

  if (mapVersion.otbmVersion >= OTBMVersion::MAP_OTBM_4)
  {
    if (item.hasAttributes())
    {
      buffer.writeU8(static_cast<uint8_t>(NodeAttribute::AttributeMap));
      serializeItemAttributeMap(item.getAttributes());
    }
  }
}

void MapIO::Serializer::serializeItemAttributeMap(const std::unordered_map<ItemAttribute_t, ItemAttribute> &attributes)
{
  // Can not have more than UINT16_MAX items
  if (attributes.size() > UINT16_MAX)
  {
    Logger::error("Saving an item with more than UINT16_MAX (65535) attributes. Only the first 65535 attributes will be saved.");
  }

  buffer.writeU16(static_cast<uint16_t>(std::min((size_t)UINT16_MAX, attributes.size())));

  auto entry = attributes.begin();

  int i = 0;
  while (entry != attributes.end() && i <= UINT16_MAX)
  {
    const ItemAttribute_t &attributeType = entry->first;
    std::stringstream attributeTypeString;
    attributeTypeString << attributeType;
    std::string s = attributeTypeString.str();
    if (s.size() > UINT16_MAX)
    {
      buffer.writeString(s.substr(0, UINT16_MAX));
    }
    else
    {
      buffer.writeString(s);
    }

    auto attribute = entry->second;
    serializeItemAttribute(attribute);
    ++entry;
    ++i;
  }
}

void MapIO::Serializer::serializeItemAttribute(ItemAttribute &attribute)
{
  if (attribute.holds<std::string>())
  {
    buffer.writeU8(AttributeTypeId::String);
    buffer.writeLongString(attribute.get<std::string>().value());
  }
  else if (attribute.holds<int>())
  {
    buffer.writeU8(AttributeTypeId::Integer);
    buffer.writeU32(static_cast<uint32_t>(attribute.get<int>().value()));
  }
  else if (attribute.holds<double>())
  {
    buffer.writeU8(AttributeTypeId::Double);
    buffer.writeU64(static_cast<uint64_t>(attribute.get<double>().value()));
  }
  else if (attribute.holds<bool>())
  {
    buffer.writeU8(AttributeTypeId::Boolean);
    buffer.writeU8(static_cast<uint8_t>(attribute.get<bool>().value()));
  }
  else
  {
    Logger::error() << "Unknown attribute when saving map: " << attribute;
  }
}

#pragma warning(pop)