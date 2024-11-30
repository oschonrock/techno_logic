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
};