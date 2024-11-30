#pragma once

#include <string>

#include <SFML/Graphics/RenderWindow.hpp>

#include "Block.hpp"

using ObjAtCoordVar = std::variant<std::monostate, Connection, std::pair<Connection, Connection>,
                                   PortRef, Ref<Node>, Ref<Gate>, Ref<BlockInst>>;
enum struct ObjAtCoordType : std::size_t {
    Empty    = 0,
    Con      = 1,
    ConCross = 2,
    Port     = 3,
    Node     = 4,
    Gate     = 5,
    Block    = 6
};
inline static constexpr std::array<std::string, 7> ObjAtCoordStrings{
    "empty", "connection", "conn crossing", "port", "node", "gate", "block"}; // for debugging

inline ObjAtCoordType typeOf(const ObjAtCoordVar& ref) { return ObjAtCoordType{ref.index()}; }

inline bool isConnectable(const ObjAtCoordVar& ref) {
    switch (ObjAtCoordType(ref.index())) {
    case ObjAtCoordType::Empty:
    case ObjAtCoordType::Con:
    case ObjAtCoordType::Port:
    case ObjAtCoordType::Node:
        return true;
    default:
        return false;
    }
}

class Editor {
  private:
    ObjAtCoordVar whatIsAtCoord(const sf::Vector2i& coord);
    bool          collisionCheck(const Connection& connection, const sf::Vector2i& coord) const;
    void          checkConEndLegal();
    [[nodiscard]] PortRef makeNewPortRef(const ObjAtCoordVar& var, const sf::Vector2i& pos,
                                         Direction dirIntoPort);

  public:
    Editor(Block& block_) : block(block_) {}

    enum struct BlockState { Idle, Connecting };
    BlockState state = BlockState::Idle;

    Block& block;

    sf::Vector2i                  conStartPos;
    sf::Vector2i                  conEndPos;
    ObjAtCoordVar                 conStartObjVar;
    ObjAtCoordVar                 conEndObjVar;
    std::optional<Ref<ClosedNet>> conStartCloNet;
    std::optional<Ref<ClosedNet>> conEndCloNet;
    bool                          conEndLegal;

    sf::Vector2i snapToGrid(const sf::Vector2f& pos) const;
    // returns wether it wants to capture the event (not allow other things to interpret it)
    bool event(const sf::Event& event, const sf::Vector2f& mousePos);
    void frame(const sf::Vector2f& mousePos);
};