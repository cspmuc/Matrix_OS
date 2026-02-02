#pragma once
#include "DisplayManager.h"

class App {
public:
    virtual ~App() {}
    virtual void draw(DisplayManager& display) = 0;
};