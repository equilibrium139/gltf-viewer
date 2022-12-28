#pragma once

struct Input
{
    float mouseX;
    float mouseY;
    float mouseDeltaX;
    float mouseDeltaY;
    
    float deltaTime;

    int windowWidth, windowHeight;

    bool wPressed, aPressed, sPressed, dPressed;
    bool leftMousePressed;
};