#pragma once
// Short contextual hints for a new player's first minutes: one line at a time, each clears when done.
namespace tutorial {
void start();             // new game
void restore(int step);   // loaded game (finished saves pass a step past the end)
int step();
void update(float dt);
const char* hint();       // current hint, or nullptr
}
