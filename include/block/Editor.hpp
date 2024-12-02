#pragma once

#include <SFML/Window/Event.hpp>
#include <string>

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

inline bool isCoordConType(const ObjAtCoordVar& ref) {
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

// Editor is responsible for storing and updating the state of the editor gui
class Editor {
  private:
    ObjAtCoordVar whatIsAtCoord(const sf::Vector2i& coord) const;
    bool          collisionCheck(const Connection& connection, const sf::Vector2i& coord) const;
    bool          isPosLegalEnd(const sf::Vector2i& pos) const;
    bool          isPosLegalStart(const sf::Vector2i& start) const;

    [[nodiscard]] PortRef makeNewPortRef(const ObjAtCoordVar& var, const sf::Vector2i& pos,
                                         Direction dirIntoPort);
    void                  splitCon(const Connection& con, Ref<Node> node);

  public:
    Editor(Block& block_) : block(block_) {}

    enum struct EditorState { Idle, Connecting };
    EditorState state = EditorState::Idle;

    Block& block;

    sf::Vector2i                  conStartPos;
    sf::Vector2i                  conEndPos;
    ObjAtCoordVar                 conStartObjVar;
    ObjAtCoordVar                 conEndObjVar;
    std::optional<Ref<ClosedNet>> conStartCloNet;
    std::optional<Ref<ClosedNet>> conEndCloNet;
    bool                          conStartLegal;
    bool                          conEndLegal;

    sf::Vector2i snapToGrid(const sf::Vector2f& pos) const; // should be in block
    void         event(const sf::Event& event, const sf::Vector2i& mousePos);
    void         frame(const sf::Vector2i& mousePos);
};