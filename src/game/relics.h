#pragma once
#include <vector>
// Hidden collectibles: golden prayer wheels tucked away off the roads (waterfall top, lake bed,
// behind trail mountains, quiet corners of the valley). Walk into one to collect it.
namespace relics {
void reset();                              // new game: nothing found
void restore(const std::vector<int>& found);
std::vector<int> found();
int count();
int total();
void update(float dt);
void draw();
bool position(int i, float& x, float& y, float& z);  // for tests
}
