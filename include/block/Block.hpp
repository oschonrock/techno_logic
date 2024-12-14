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
  protected:
    bool collisionCheck(const Connection& con, const sf::Vector2i& coord) const;

    [[nodiscard]] PortRef makeNewPortRef(const sf::Vector2i& pos, Direction portDir);
    void                  insertCon(const Connection& con);
    void                  splitCon(const Connection& con, Ref<Node> node);
    void                  updateNode(Ref<Node> node);

  public:
    Block(std::string name_, std::size_t size_) : name(name_), size(size_) {}

    std::string name;
    std::string description;
    std::size_t size = 100;

    StableVector<BlockInst> blockInstances;
    StableVector<Node>      nodes;
    StableVector<Gate>      gates;
    StableVector<ClosedNet> nets;
    std::vector<Port>       ports;

    // PortInst&                     getPort(const PortRef& port);
    const PortInst&               getPort(const PortRef& port) const;
    PortType                      getPortType(const PortRef& port) const;
    std::pair<PortType, PortType> getPortType(const Connection& con) const;
    [[nodiscard]] ObjAtCoordVar   whatIsAtCoord(const sf::Vector2i& coord) const;

    template <typename T>
    [[nodiscard]] std::optional<Ref<ClosedNet>> getClosNetRef(const T& obj) const {
        for (const auto& net: nets) {
            if (net.obj.contains(obj)) return net.ind;
        }
        return {};
    }
    template <typename T>
    [[nodiscard]] bool contains(const T& obj) const {
        return getClosNetRef(obj).has_value();
    }
    [[nodiscard]] std::size_t getNodeConCount(const Ref<Node>& node) const;

    Connection addConnection(const sf::Vector2i& startPos, const sf::Vector2i& endPos);
    void insertOverlap(const Connection& con1, const Connection& con2, const sf::Vector2i& pos);
    void eraseCon(const Connection& con);
};