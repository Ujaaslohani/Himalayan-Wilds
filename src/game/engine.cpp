#include "engine.h"
#include <random>
#include <cstring>

Engine E;
App A;

namespace {
struct ItemDef { const char *id, *name, *label, *desc, *icon; };
const ItemDef ITEMS[] = {
    {"bow", "Mountain Yew Bow", "Bow", "Hold left mouse to draw, release to loose. Hold right mouse to aim.", "bow"},
    {"arrow", "Flint Arrows", "Arrows", "Craft 10 from a single timber log.", "arrow"},
    {"wood", "Timber", "Timber", "Fallen deodar logs from the pine woods.", "wood"},
    {"stone", "Slate", "Slate", "Blue-grey slate for Kath-Kuni foundations and roofs.", "stone"},
    {"fiber", "Nettle Fibre", "Nettle Fibre", "Braided cordage from mountain nettle.", "fiber"},
    {"herb", "Artemisia Herbs", "Herbs", "Aromatic alpine herb used by healers.", "herb"},
    {"deer_pelt", "Deer Pelt", "Deer Pelt", "Warm chestnut fur for garments and bindings.", "deer"},
    {"boar_hide", "Boar Hide", "Boar Hide", "Tough leather for shelters and gear.", "boar"},
    {"goral_horn", "Goral Horn", "Goral Horn", "Curved horn prized by craftsmen.", "goral"},
    {"bone", "Bone", "Bone", "For arrowheads, hooks and tool handles.", "bone"},
    {"sinew", "Sinew", "Sinew", "Elastic bowstring cordage.", "sinew"},
    {"food", "Food", "Food", "Dried meat, grain and herbs. Each resident eats one at dawn, half as much again in winter.", "food"},
    {"fur_coat", "Fur Coat", "Fur Coat", "Keeps the winter cold away at night.", "coat"},
    {"leather_armour", "Leather Armour", "Leather Armour", "Predators deal 40% less damage.", "armour"},
    {"wolf_fur", "Wolf Fur", "Wolf Fur", "Thick grey winter fur. A prized trophy.", "wolf"},
    {"bear_hide", "Bear Hide", "Bear Hide", "Heavy black hide with a cream chest mark. A rare trophy.", "bear"},
    {"leopard_pelt", "Leopard Pelt", "Leopard Pelt", "Golden rosetted pelt from the high slopes.", "leopard"},
};
const ItemDef* findItem(const std::string& id) {
  for (auto& i : ITEMS)
    if (id == i.id) return &i;
  return nullptr;
}
std::vector<std::pair<std::string, std::function<void(std::vector<Marker>&)>>> g_markerSources;
std::mt19937 g_rng(12345);
}  // namespace

const char* ITEM_ORDER[] = {"bow", "arrow", "wood", "stone", "fiber", "herb", "deer_pelt", "boar_hide", "goral_horn", "bone", "sinew", "food", "fur_coat", "leather_armour", "wolf_fur", "bear_hide", "leopard_pelt"};
const int ITEM_COUNT = sizeof(ITEM_ORDER) / sizeof(ITEM_ORDER[0]);

const char* itemLabel(const std::string& id) { auto* i = findItem(id); return i ? i->label : id.c_str(); }
const char* itemName(const std::string& id) { auto* i = findItem(id); return i ? i->name : id.c_str(); }
const char* itemDesc(const std::string& id) { auto* i = findItem(id); return i ? i->desc : ""; }
const char* itemIcon(const std::string& id) { auto* i = findItem(id); return i ? i->icon : "box"; }

float randf() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(g_rng); }
float frand() { return randf(); }

int invCount(const std::string& id) {
  auto it = A.inv.find(id);
  return it == A.inv.end() ? 0 : it->second;
}

void emitEvent(const HudEvent& e) {
  E.events.push_back(e);
  if (E.events.size() > 24) E.events.erase(E.events.begin());
}

void popup(const std::string& text, uint32_t color) {
  HudEvent e;
  e.kind = HudEvent::Popup;
  e.text = text;
  e.color = color;
  emitEvent(e);
}

uint32_t rgbHex(const char* hex) {
  if (*hex == '#') hex++;
  unsigned v = 0;
  for (int i = 0; i < 6 && hex[i]; i++) {
    char ch = hex[i];
    v = v * 16 + (unsigned)(ch >= '0' && ch <= '9' ? ch - '0' : (ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch - 'A' + 10));
  }
  return ((v >> 16) & 255) | (((v >> 8) & 255) << 8) | ((v & 255) << 16) | (255u << 24);
}

void makeNoise(float x, float z, float r) {
  E.noise.x = x;
  E.noise.z = z;
  E.noise.r = r;
  E.noise.t = E.clock;
}

float relativeBearing(float x, float z) {
  const Vec3& p = E.player.pos;
  float worldAngle = std::atan2(-(x - p.x), -(z - p.z));
  return wrapAngle(worldAngle - E.player.yaw);
}

void registerMarkers(const std::string& key, std::function<void(std::vector<Marker>&)> fn) {
  for (auto& s : g_markerSources)
    if (s.first == key) { s.second = fn; return; }
  g_markerSources.push_back({key, fn});
}

void collectMarkers(std::vector<Marker>& out) {
  out.clear();
  for (auto& s : g_markerSources) s.second(out);
}

namespace actions {
void later(double seconds, std::function<void()> fn) { A.timers.push_back({E.clock + seconds, fn}); }
}
