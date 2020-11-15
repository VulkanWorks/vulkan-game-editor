#pragma once

#include <array>
#include <memory>
#include <stdint.h>

#include "const.h"
#include "position.h"
#include "tile_location.h"

class MapIterator;
class Map;

class Floor
{
public:
	Floor(int x, int y, int z);

	Floor(const Floor &) = delete;
	Floor &operator=(const Floor &) = delete;

	Floor(Floor &&other) noexcept;
	Floor &operator=(Floor &&other) noexcept;

	TileLocation &getTileLocation(int x, int y);
	TileLocation &getTileLocation(uint32_t index);

private:
	// x, y locations
	std::array<TileLocation, MAP_TREE_CHILDREN_COUNT> locations{};
};

namespace quadtree
{
	class Node
	{
		enum class NodeType
		{
			Root,
			Node,
			Leaf
		};

	public:
		class Children
		{
		public:
			static constexpr size_t Amount = MAP_TREE_CHILDREN_COUNT;

			Children(NodeType type);
			~Children();

			Children(Children &&other);

			Children &operator=(Children &&other);

			void construct();
			void destruct();
			void reset();

			size_t size() const noexcept;

			void setNode(size_t index, std::unique_ptr<Node> &&value);
			void setFloor(size_t index, std::unique_ptr<Floor> &&value);

			std::unique_ptr<Node> &nodePtr(size_t index);
			std::unique_ptr<Floor> &floorPtr(size_t index);

			Node *node(size_t index) const;
			Floor *floor(size_t index) const;

		private:
			NodeType _type;
			union
			{
				std::unique_ptr<Node> nodes[Amount];
				std::unique_ptr<Floor> floors[Amount];
			};
		};

		Node(NodeType nodeType);
		Node(NodeType nodeType, int level);

		Node(Node &&other) noexcept;
		Node &operator=(Node &&other) noexcept;

		void clear();

		int level = -1;

		Node(const Node &) = delete;
		Node &operator=(const Node &) = delete;

		// Get a leaf node. Creates the leaf node if it does not already exist.
		Node &getLeafWithCreate(int x, int y);
		Node *getLeafUnsafe(int x, int y) const;
		TileLocation *getTile(int x, int y, int z) const;

		Floor &getOrCreateFloor(Position pos);
		Floor &getOrCreateFloor(int x, int y, int z);
		Floor *floor(uint32_t z) const;

		TileLocation &getOrCreateTileLocation(Position pos);

		inline bool isLeaf() const noexcept;
		inline bool isRoot() const noexcept;

		friend class ::Map;
		friend class ::MapIterator;

	protected:
		NodeType nodeType = NodeType::Root;
		Children children;
	};
}; // namespace quadtree

inline bool quadtree::Node::isLeaf() const noexcept
{
	return nodeType == NodeType::Leaf;
}

inline bool quadtree::Node::isRoot() const noexcept
{
	return nodeType == NodeType::Root;
}