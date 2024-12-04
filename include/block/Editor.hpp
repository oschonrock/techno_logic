#pragma once

#include <SFML/Window/Event.hpp>

#include "Block.hpp"

// Editor is responsible for storing and updating the state of the editor gui
class Editor {
  private:
    bool isPosLegalEnd(const sf::Vector2i& pos) const;
    bool isPosLegalStart(const sf::Vector2i& start) const;

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
    std::vector<sf::Vector2i>     overlapPos;

    sf::Vector2i snapToGrid(const sf::Vector2f& pos) const; // should be in block
    void         event(const sf::Event& event, const sf::Vector2i& mousePos);
    void         frame(const sf::Vector2i& mousePos);
};