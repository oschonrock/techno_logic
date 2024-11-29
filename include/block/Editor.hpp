#pragma once

#include <string>

#include <SFML/Graphics/RenderWindow.hpp>

#include "Block.hpp"

using ObjAtCoordVar =
    std::variant<std::monostate, Connection, PortRef, Ref<Node>, Ref<Gate>, Ref<BlockInst>>;
enum struct ObjAtCoordType : std::size_t {
    Empty = 0,
    Con   = 1,
    Port  = 2,
    Node  = 3,
    Gate  = 4,
    Block = 5
};
inline static constexpr std::array<std::string, 6> ObjAtCoordStrings{
    "empty", "connection", "port", "node", "gate", "block"}; // for debugging

inline ObjAtCoordType typeOf(const ObjAtCoordVar& ref) { return ObjAtCoordType{ref.index()}; }

class Editor {
  private:
    ObjAtCoordVar whatIsAtCoord(const sf::Vector2i& coord);
    bool          collisionCheck(const Connection& connection, const sf::Vector2i& coord) const;
    void          checkConLegal();
    [[nodiscard]] PortRef makeNewPortRef(const ObjAtCoordVar& var, const sf::Vector2i& pos,
                                         Direction dirIntoPort);

  public:
    Editor(Block& block_) : block(block_) {}

    PortInst&       getPort(const PortRef& port);
    const PortInst& getPort(const PortRef& port) const;

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