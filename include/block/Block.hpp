#pragma once

#include "BlockInternals.hpp"

struct Block {
    std::size_t size = 100;
    std::string name;
    std::string description;

    StableVector<BlockInst> blockInstances;
    StableVector<Node>      nodes;
    StableVector<Gate>      gates;
    ConnectionNetwork       conNet;

    PortInst& getPort(const PortRef& port) {
        switch (typeOf(port.ref)) {
        case PortObjType::Node:
            return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
        case PortObjType::Gate:
            return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
        case PortObjType::BlockInst:
            return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
        }
    }
    const PortInst& getPort(const PortRef& port) const {
        switch (typeOf(port.ref)) {
        case PortObjType::Node:
            return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
        case PortObjType::Gate:
            return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
        case PortObjType::BlockInst:
            return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
        }
    }

    void splitConWithNode(const Connection& con, const Ref<Node>& nodeRef) {
        assert(conNet.contains(con) && !conNet.contains(nodeRef));
        auto&     net = conNet.nets[conNet.getClosNetRef(con.portRef1).value()];
        Direction dir = vecToDir(nodes[nodeRef].pos - getPort(con.portRef1).portPos);
        net.erase(con);
        net.insert(Connection(con.portRef1,
                              PortRef{nodeRef, static_cast<std::size_t>(dir), PortType::node}));
        net.insert(Connection(
            con.portRef2,
            PortRef{nodeRef, static_cast<std::size_t>(reverseDirection(dir)), PortType::node}));
    }

    bool collisionCheck(const Connection& con, const sf::Vector2i& coord) const {
        return isVecBetween(coord, getPort(con.portRef1).portPos, getPort(con.portRef2).portPos);
    }
};