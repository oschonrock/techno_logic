#include "BlockInternals.hpp"

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

class Block {
  private:
    bool collisionCheck(const Connection& con, const sf::Vector2i& coord) const;
    void splitCon(const Connection& con, Ref<Node> node);

  public:
    std::size_t size = 100;
    std::string name;
    std::string description;

    StableVector<BlockInst> blockInstances;
    StableVector<Node>      nodes;
    StableVector<Gate>      gates;
    ConnectionNetwork       conNet;
    std::vector<Port>       ports;

    PortInst&                     getPort(const PortRef& port);
    const PortInst&               getPort(const PortRef& port) const;
    PortType                      getPortType(const PortRef& port) const;
    std::pair<PortType, PortType> getPortType(const Connection& con) const; // TODO remove
    [[nodiscard]] ObjAtCoordVar   whatIsAtCoord(const sf::Vector2i& coord) const;

    // TODO possibly should just be in editor
    [[nodiscard]] std::vector<sf::Vector2i>
    getOverlapPos(std::pair<sf::Vector2i, sf::Vector2i> line, Ref<ClosedNet> netRef) const;
    [[nodiscard]] std::vector<sf::Vector2i> getOverlapPos(Ref<ClosedNet> net1,
                                                          Ref<ClosedNet> net2) const;

    void insertCon(const Connection& con, const std::optional<Ref<ClosedNet>>& net1,
                   const std::optional<Ref<ClosedNet>>& net2);
    void insertOverlap(const Connection& con1, const Connection& con2, const sf::Vector2i& pos);
    [[nodiscard]] PortRef makeNewPortRef(ObjAtCoordVar& var, const sf::Vector2i& pos,
                                         Direction dirIntoPort);
};