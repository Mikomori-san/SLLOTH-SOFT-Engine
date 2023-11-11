#pragma once
#include "GraphicsComponent.h"

class AnimatedGC : public GraphicsComponent {
private:
    int rows;
    int columns;
    std::string specificComponentId = "animatedGC";

public:
    AnimatedGC(std::weak_ptr<GameObject> gameObject, const sf::Sprite& sprite, int rows, int columns)
        : GraphicsComponent(gameObject, sprite), rows(rows), columns(columns) {}
    
    void update() override;
    
    void receive(int message) override;

    std::string Component::getSpecificComponentId()
    {
        return specificComponentId;
    }
};