#pragma once

#include <string>

#include <SFML/Graphics/RenderWindow.hpp>

#include "BlockInternals.hpp"

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

class Block {
  private:
    static constexpr float        nodeRad            = 0.1f;
    inline static const sf::Color nodeColour         = sf::Color::White;
    inline static const sf::Color conColour          = sf::Color::White;
    inline static const sf::Color newConColour       = sf::Color::Blue;
    inline static const sf::Color highlightConColour = sf::Color::Green;

    enum struct BlockState { Idle, Connecting };
    BlockState state = BlockState::Idle;

    ObjAtCoordVar   whatIsAtCoord(const sf::Vector2i& coord);
    PortInst&       getPort(const PortRef& port);
    const PortInst& getPort(const PortRef& port) const;
    bool            collisionCheck(const Connection& connection, const sf::Vector2i& coord) const;
    void            checkConLegal();
    [[nodiscard]] PortRef makeNewPortRef(const ObjAtCoordVar& var, const sf::Vector2i& pos,
                                         Direction dirIntoPort);

    sf::Vector2i                  conStartPos;
    sf::Vector2i                  conEndPos;
    ObjAtCoordVar                 conStartObjVar;
    ObjAtCoordVar                 conEndObjVar;
    std::optional<Ref<ClosedNet>> conStartCloNet;
    std::optional<Ref<ClosedNet>> conEndCloNet;
    bool                          conEndLegal;

  public:
    std::size_t size = 100;
    std::string name;
    std::string description;

    StableVector<BlockInst> blockInstances;
    StableVector<Node>      nodes;
    StableVector<Gate>      gates;
    ConnectionNetwork       conNet;

    sf::Vector2i snapToGrid(const sf::Vector2f& pos) const;
    // returns wether it wants to capture the event (not allow other things to interpret it)
    bool event(const sf::Event& event, sf::RenderWindow& window, const sf::Vector2f& mousePos);
    void frame(sf::RenderWindow& window, const sf::Vector2f& mousePos);

  private:
    void draw(sf::RenderWindow& window);
};