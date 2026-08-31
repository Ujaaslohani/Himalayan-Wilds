#include "platform/platform.h"
#include "console/console.h"
#include "console/engineAPI.h"
#include "console/sim.h"
#include "console/simSet.h"
#include "core/util/str.h"
#include "core/volume.h"
#include "forest/forest.h"
#include "forest/forestDataFile.h"
#include "forest/forestItem.h"
#include "math/mPoint2.h"
#include "math/mPoint3.h"
#include "materials/materialDefinition.h"
#include "T3D/Scene.h"
#include "T3D/aiPlayer.h"
#include "terrain/terrData.h"
#include "terrain/terrFile.h"
#include "terrain/terrMaterial.h"

#include <cmath>

namespace
{
   constexpr const char* TerrainName = "HSPFoundationTerrain";
   constexpr const char* ForestName = "HSPFoundationForest";
   constexpr const char* StreamName = "HSPFoundationStream";
   constexpr const char* ForestItemDataName = "HSPFoundationTreeItemData";
   constexpr const char* ForestDataFileName = "data/HSPFoundation/generated/HSPFoundationForest.forest";
   constexpr const char* AiName = "HSPFoundationAIPlayer";
   constexpr const char* PlayerDataName = "HSPFoundationPlayerData";
   constexpr const char* VisualMarkerPrefix = "HSPFoundationVisualMarker";
   constexpr U32 TerrainResolution = 256;
   constexpr F32 TerrainSquareSize = 4.0f;
   constexpr F32 TerrainWorldOriginX = -512.0f;
   constexpr F32 TerrainWorldOriginY = -512.0f;

   struct StreamNode
   {
      F32 X;
      F32 Y;
   };

   constexpr StreamNode FoundationStreamPath[] =
   {
      { -205.0f, -150.0f },
      { -160.0f, -128.0f },
      { -112.0f, -104.0f },
      { -65.0f, -82.0f },
      { -20.0f, -58.0f },
      { 40.0f, -36.0f },
      { 95.0f, -12.0f },
   };

   struct FoundationFeature
   {
      const char* Name;
      bool EnabledByDefault;
      const char* Notes;
   };

   struct FoundationFeatureOverride
   {
      String Name;
      bool Enabled;
   };

   const FoundationFeature FoundationFeatures[] =
   {
      { "TerrainPrototype", true, "C++ generated terrain used by the foundation validation scene." },
      { "FirstPersonPlayer", true, "Native Torque Player spawn path for the controllable character." },
      { "FoundationForestVegetation", true, "Level-owned Torque Forest using generated placement data and removable item datablocks." },
      { "FoundationWaterStream", true, "Level-owned Torque River object for stream validation; no custom water renderer." },
      { "FoundationAIPlayer", false, "Disabled for this gate; AI resumes after player/camera/terrain validation." },
      { "FoundationVisualMarkers", false, "Optional landmarks; disabled while terrain/camera validation is in progress." },
      { "BuilderTimedConstruction", true, "Future building intent is completed by builder time, not player material hauling." },
      { "MaterialGatheringEconomy", false, "Disabled for now: no mandatory wood, stone, or iron collection loop." },
   };

   Vector<FoundationFeatureOverride>& featureOverrides()
   {
      static Vector<FoundationFeatureOverride> overrides;
      return overrides;
   }

   bool findFoundationFeature(const char* featureName, const FoundationFeature** outFeature)
   {
      if (!featureName || !featureName[0])
         return false;

      for (const FoundationFeature& feature : FoundationFeatures)
      {
         if (dStricmp(feature.Name, featureName) == 0)
         {
            if (outFeature)
               *outFeature = &feature;
            return true;
         }
      }

      return false;
   }

   bool isFoundationFeatureEnabled(const char* featureName)
   {
      const FoundationFeature* feature = nullptr;
      if (!findFoundationFeature(featureName, &feature))
         return false;

      auto& overrides = featureOverrides();
      for (const FoundationFeatureOverride& featureOverride : overrides)
      {
         if (dStricmp(featureOverride.Name.c_str(), feature->Name) == 0)
            return featureOverride.Enabled;
      }

      return feature->EnabledByDefault;
   }

   F32 hspHeightAtGrid(U32 x, U32 y)
   {
      const F32 cx = (F32)x - (TerrainResolution * 0.5f);
      const F32 cy = (F32)y - (TerrainResolution * 0.5f);
      const F32 dist = std::sqrt((cx * cx) + (cy * cy));

      const F32 ridge = 24.0f * std::sin(((F32)x * 0.045f) + ((F32)y * 0.025f));
      const F32 shoulder = 18.0f * std::cos(((F32)y * 0.052f) - 0.8f);
      const F32 mountain = mClampF(95.0f - (dist * 0.55f), 0.0f, 95.0f);
      F32 height = 14.0f + mountain + ridge + shoulder;

      // Keep a small playable clearing near the origin while retaining slopes around it.
      if (dist < 32.0f)
      {
         const F32 t = mClampF(dist / 32.0f, 0.0f, 1.0f);
         const F32 clearingHeight = 18.0f + (cy * 0.025f);
         height = (clearingHeight * (1.0f - t)) + (height * t);
      }

      const F32 worldX = TerrainWorldOriginX + ((F32)x * TerrainSquareSize);
      const F32 worldY = TerrainWorldOriginY + ((F32)y * TerrainSquareSize);
      F32 streamDistance = 100000.0f;
      for (U32 i = 1; i < (sizeof(FoundationStreamPath) / sizeof(FoundationStreamPath[0])); ++i)
      {
         const StreamNode& a = FoundationStreamPath[i - 1];
         const StreamNode& b = FoundationStreamPath[i];
         const F32 vx = b.X - a.X;
         const F32 vy = b.Y - a.Y;
         const F32 wx = worldX - a.X;
         const F32 wy = worldY - a.Y;
         const F32 lenSq = (vx * vx) + (vy * vy);
         const F32 t = lenSq > 0.001f ? mClampF(((wx * vx) + (wy * vy)) / lenSq, 0.0f, 1.0f) : 0.0f;
         const F32 dx = worldX - (a.X + (vx * t));
         const F32 dy = worldY - (a.Y + (vy * t));
         streamDistance = getMin(streamDistance, std::sqrt((dx * dx) + (dy * dy)));
      }

      if (streamDistance < 42.0f)
      {
         const F32 channel = 1.0f - mClampF(streamDistance / 42.0f, 0.0f, 1.0f);
         const F32 bankBlend = channel * channel * (3.0f - (2.0f * channel));
         height -= bankBlend * 18.0f;
      }

      return mClampF(height, 2.0f, 155.0f);
   }

   F32 hspHeightAtWorld(F32 x, F32 y)
   {
      const F32 localX = (x - TerrainWorldOriginX) / TerrainSquareSize;
      const F32 localY = (y - TerrainWorldOriginY) / TerrainSquareSize;
      const U32 gridX = (U32)mClampF(mFloor(localX), 0.0f, (F32)(TerrainResolution - 1));
      const U32 gridY = (U32)mClampF(mFloor(localY), 0.0f, (F32)(TerrainResolution - 1));
      return hspHeightAtGrid(gridX, gridY);
   }

   TerrainBlock* findFoundationTerrain()
   {
      TerrainBlock* terrain = nullptr;
      Sim::findObject(TerrainName, terrain);
      return terrain;
   }

   F32 sampleFoundationHeight(F32 x, F32 y)
   {
      TerrainBlock* terrain = findFoundationTerrain();
      if (!terrain)
         return hspHeightAtWorld(x, y);

      Point3F terrainPos;
      terrain->getTransform().getColumn(3, &terrainPos);

      F32 height = 0.0f;
      const Point2F localPos(x - terrainPos.x, y - terrainPos.y);
      terrain->getHeight(localPos, &height);
      return height + terrainPos.z;
   }

   void addToMissionCleanup(SimObject* object)
   {
      SimGroup* cleanup = nullptr;
      if (object && Sim::findObject("MissionCleanup", cleanup))
         cleanup->addObject(object);
   }

   void deleteVisualMarker(const char* name)
   {
      SceneObject* marker = nullptr;
      if (Sim::findObject(name, marker))
         marker->deleteObject();
   }

   void spawnVisualMarker(const char* name, const char* shapeAsset, const Point3F& position, const Point3F& scale)
   {
      const String props = String::ToString(
         "shapeAsset = \"%s\"; position = \"%0.3f %0.3f %0.3f\"; scale = \"%0.3f %0.3f %0.3f\";",
         shapeAsset,
         position.x, position.y, position.z,
         scale.x, scale.y, scale.z);

      SimObject* spawned = Sim::spawnObject("TSStatic", String::EmptyString, name, props, String::EmptyString);
      SceneObject* marker = dynamic_cast<SceneObject*>(spawned);
      if (!marker)
      {
         Con::warnf("HSPFoundation: failed to spawn visual marker '%s'", name);
         return;
      }

      if (Scene* scene = Scene::getRootScene())
         scene->addObject(marker);
      addToMissionCleanup(marker);
   }

   bool prepareFoundationTerrainAsset()
   {
      if (!isFoundationFeatureEnabled("TerrainPrototype"))
      {
         Con::warnf("HSPFoundation: TerrainPrototype feature is disabled");
         return false;
      }

   Platform::createPath("data/HSPFoundation/generated/");

   Vector<String> materials;
   materials.push_back("HSPFoundationTerrainGrass");

      String terrainFileName("data/HSPFoundation/generated/HSPFoundationTerrain.ter");
      Torque::FS::Remove(Torque::Path(terrainFileName));
      TerrainFile::create(&terrainFileName, TerrainResolution, materials);

      TerrainBlock* terrain = new TerrainBlock();
      if (!terrain->setFile(terrainFileName))
      {
         Con::errorf("HSPFoundation: failed to create terrain file '%s'", terrainFileName.c_str());
         delete terrain;
         return false;
      }

      TerrainFile* file = terrain->getFile();
      for (U32 y = 0; y < TerrainResolution; ++y)
      {
         for (U32 x = 0; x < TerrainResolution; ++x)
            file->setHeight(x, y, floatToFixed(hspHeightAtGrid(x, y)));
      }

      terrain->setDataField(StringTable->insert("squareSize"), nullptr, "4");
      terrain->updateGrid(Point2I::Zero, Point2I(TerrainResolution, TerrainResolution));
      terrain->updateGridMaterials(Point2I::Zero, Point2I(TerrainResolution, TerrainResolution));
      terrain->save(terrainFileName.c_str());
      delete terrain;

      Con::printf("HSPFoundation: prepared persistent terrain file '%s' (%u x %u, squareSize %0.1f)",
         terrainFileName.c_str(), TerrainResolution, TerrainResolution, TerrainSquareSize);
      return true;
   }

   bool prepareFoundationForestAsset()
   {
      if (!isFoundationFeatureEnabled("FoundationForestVegetation"))
      {
         Con::warnf("HSPFoundation: FoundationForestVegetation feature is disabled");
         return false;
      }

      ForestItemData* treeData = ForestItemData::find(ForestItemDataName);
      if (!treeData)
      {
         Con::errorf("HSPFoundation: cannot prepare forest; ForestItemData '%s' is not registered", ForestItemDataName);
         return false;
      }

      Platform::createPath("data/HSPFoundation/generated/");
      Torque::FS::Remove(Torque::Path(ForestDataFileName));

      ForestData forestData;
      struct Placement
      {
         F32 X;
         F32 Y;
         F32 Rotation;
         F32 Scale;
      };

      const Placement placements[] =
      {
         { -172.0f, -74.0f, 0.20f, 5.0f },
         { -151.0f, -48.0f, 1.70f, 4.2f },
         { -132.0f, -30.0f, 3.10f, 4.8f },
         { -96.0f, -162.0f, 0.70f, 3.8f },
         { -68.0f, -139.0f, 2.40f, 4.5f },
         { -41.0f, -117.0f, 4.00f, 3.5f },
         { 22.0f, -83.0f, 5.30f, 4.1f },
         { 48.0f, -61.0f, 1.10f, 5.4f },
         { 77.0f, -36.0f, 2.90f, 4.7f },
         { -185.0f, 12.0f, 0.90f, 4.3f },
         { -148.0f, 39.0f, 3.70f, 5.2f },
         { -113.0f, 58.0f, 2.10f, 3.9f },
         { 88.0f, 52.0f, 4.80f, 4.6f },
         { 121.0f, 78.0f, 1.40f, 5.1f },
         { 154.0f, 101.0f, 2.70f, 4.4f },
      };

      for (const Placement& placement : placements)
      {
         const F32 z = sampleFoundationHeight(placement.X, placement.Y);
         forestData.addItem(treeData, Point3F(placement.X, placement.Y, z), placement.Rotation, placement.Scale);
      }

      if (!forestData.write(ForestDataFileName))
      {
         Con::errorf("HSPFoundation: failed to write forest data file '%s'", ForestDataFileName);
         return false;
      }

      Con::printf("HSPFoundation: prepared persistent forest file '%s' with %u placeholder trees",
         ForestDataFileName, (U32)(sizeof(placements) / sizeof(placements[0])));
      return true;
   }

   void logTerrainMaterialState(const char* label, TerrainBlock* terrain)
   {
      if (!terrain || terrain->getTerrainMaterialCount() == 0)
      {
         Con::warnf("HSPFoundation: material[%s] no terrain materials available", label ? label : "unspecified");
         return;
      }

      TerrainMaterial* terrainMaterial = terrain->getMaterial(0);
      Material* renderMaterial = nullptr;
      Sim::findObject("HSPFoundationTerrainGrassRenderMaterial", renderMaterial);

      Con::printf("HSPFoundation: material[%s] terrainName='%s' diffuseAsset='%s' renderMaterial=%s renderMapTo='%s'",
         label ? label : "unspecified",
         terrain->getTerrainMaterialName(0),
         terrainMaterial ? terrainMaterial->getDiffuseMapAssetId() : "",
         renderMaterial ? "yes" : "no",
         renderMaterial ? renderMaterial->mMapTo.c_str() : "");
   }
}

DefineEngineFunction(HSP_prepareFoundationTerrainAsset, bool, (), ,
   "Generates or refreshes the persistent terrain file used by the mission-owned TerrainBlock.")
{
   return prepareFoundationTerrainAsset();
}

DefineEngineFunction(HSP_createFoundationTerrain, bool, (), ,
   "Compatibility wrapper. Prepares the persistent terrain file; the mission owns the TerrainBlock.")
{
   return prepareFoundationTerrainAsset();
}

DefineEngineFunction(HSP_prepareFoundationForestAsset, bool, (), ,
   "Generates or refreshes the persistent forest placement file used by the mission-owned Forest.")
{
   return prepareFoundationForestAsset();
}

DefineEngineFunction(HSP_getFoundationTerrainHeight, F32, (F32 x, F32 y), ,
   "Returns the generated foundation terrain height at the supplied world X/Y.")
{
   return sampleFoundationHeight(x, y);
}

DefineEngineFunction(HSP_snapObjectToFoundationTerrain, bool, (SimObjectId objectId, F32 clearance), (0.4f),
   "Places a SceneObject on the generated foundation terrain with a small clearance.")
{
   SceneObject* object = nullptr;
   if (!Sim::findObject(objectId, object))
      return false;

   Point3F pos = object->getPosition();
   pos.z = sampleFoundationHeight(pos.x, pos.y) + clearance;
   object->setPosition(pos);
   return true;
}

DefineEngineFunction(HSP_createFoundationVisualMarkers, bool, (), ,
   "Creates simple visible landmarks for foundation validation.")
{
   if (!isFoundationFeatureEnabled("FoundationVisualMarkers"))
   {
      Con::warnf("HSPFoundation: FoundationVisualMarkers feature is disabled");
      return false;
   }

   deleteVisualMarker("HSPFoundationVisualMarker_Clearing");
   deleteVisualMarker("HSPFoundationVisualMarker_Slope");
   deleteVisualMarker("HSPFoundationVisualMarker_AITarget");

   const F32 clearingHeight = sampleFoundationHeight(-42.0f, -18.0f);
   const F32 slopeHeight = sampleFoundationHeight(58.0f, 42.0f);
   const F32 targetHeight = sampleFoundationHeight(82.0f, 48.0f);

   spawnVisualMarker("HSPFoundationVisualMarker_Clearing", "Prototyping:CubePrimitive_shape",
      Point3F(-42.0f, -18.0f, clearingHeight + 0.35f), Point3F(4.0f, 4.0f, 0.12f));
   spawnVisualMarker("HSPFoundationVisualMarker_Slope", "Prototyping:ConePrimitive_shape",
      Point3F(58.0f, 42.0f, slopeHeight + 1.2f), Point3F(1.5f, 1.5f, 2.0f));
   spawnVisualMarker("HSPFoundationVisualMarker_AITarget", "Prototyping:SpherePrimitive_shape",
      Point3F(82.0f, 48.0f, targetHeight + 1.2f), Point3F(1.1f, 1.1f, 1.1f));

   Con::printf("HSPFoundation: created visible foundation markers near the player spawn and AI target");
   return true;
}

DefineEngineFunction(HSP_spawnFoundationAI, S32, (), ,
   "Spawns one AIPlayer for foundation validation and gives it a simple movement target.")
{
   if (!isFoundationFeatureEnabled("FoundationAIPlayer"))
   {
      Con::warnf("HSPFoundation: FoundationAIPlayer feature is disabled");
      return 0;
   }

   if (AIPlayer* existing = nullptr; Sim::findObject(AiName, existing))
      existing->deleteObject();

   const Point3F spawn(72.0f, 44.0f, sampleFoundationHeight(72.0f, 44.0f) + 1.0f);
   const String props = String::ToString("position = \"%0.3f %0.3f %0.3f\";", spawn.x, spawn.y, spawn.z);
   SimObject* spawned = Sim::spawnObject("AIPlayer", PlayerDataName, AiName, props, String::EmptyString);
   AIPlayer* ai = dynamic_cast<AIPlayer*>(spawned);
   if (!ai)
   {
      Con::errorf("HSPFoundation: failed to spawn AIPlayer with datablock '%s'", PlayerDataName);
      return 0;
   }

   addToMissionCleanup(ai);
   ai->setMoveSpeed(0.35f);
   ai->setMoveDestination(Point3F(82.0f, 48.0f, sampleFoundationHeight(82.0f, 48.0f) + 1.0f), true);
   Con::printf("HSPFoundation: spawned AIPlayer '%s' and assigned a movement target", AiName);
   return ai->getId();
}

DefineEngineFunction(HSP_isFoundationFeatureEnabled, bool, (const char* featureName), ,
   "Returns whether a foundation feature is currently enabled.")
{
   return isFoundationFeatureEnabled(featureName);
}

DefineEngineFunction(HSP_setFoundationFeatureEnabled, bool, (const char* featureName, bool enabled), ,
   "Overrides a foundation feature flag for validation builds.")
{
   const FoundationFeature* feature = nullptr;
   if (!findFoundationFeature(featureName, &feature))
   {
      Con::errorf("HSPFoundation: unknown foundation feature '%s'", featureName ? featureName : "");
      return false;
   }

   auto& overrides = featureOverrides();
   for (FoundationFeatureOverride& featureOverride : overrides)
   {
      if (dStricmp(featureOverride.Name.c_str(), feature->Name) == 0)
      {
         featureOverride.Enabled = enabled;
         Con::printf("HSPFoundation: feature '%s' set to %s", feature->Name, enabled ? "enabled" : "disabled");
         return true;
      }
   }

   FoundationFeatureOverride featureOverride;
   featureOverride.Name = feature->Name;
   featureOverride.Enabled = enabled;
   overrides.push_back(featureOverride);
   Con::printf("HSPFoundation: feature '%s' set to %s", feature->Name, enabled ? "enabled" : "disabled");
   return true;
}

DefineEngineFunction(HSP_logFoundationFeaturePolicy, void, (), ,
   "Logs the current modular feature policy for the foundation build.")
{
   Con::printf("HSPFoundation: feature policy begin");
   for (const FoundationFeature& feature : FoundationFeatures)
   {
      Con::printf("HSPFoundation: feature %-28s %s - %s",
         feature.Name,
         isFoundationFeatureEnabled(feature.Name) ? "enabled " : "disabled",
         feature.Notes);
   }
   Con::printf("HSPFoundation: feature policy end");
}

DefineEngineFunction(HSP_runFoundationDiagnostics, bool, (const char* label), ("unspecified"),
   "Logs the minimal foundation validation state.")
{
   TerrainBlock* terrain = findFoundationTerrain();
   if (!terrain)
   {
      Con::errorf("HSPFoundation: diagnostics[%s] failed; mission-owned terrain is missing", label ? label : "unspecified");
      return false;
   }

   F32 minHeight = 0.0f;
   F32 maxHeight = 0.0f;
   terrain->getMinMaxHeight(&minHeight, &maxHeight);

   const F32 spawnX = -120.0f;
   const F32 spawnY = -110.0f;
   const F32 spawnHeight = sampleFoundationHeight(spawnX, spawnY);

   Con::printf("HSPFoundation: diagnostics[%s] terrainId=%u isServer=%u isClient=%u terrainAsset='%s' terrainFile='%s' blockSize=%u squareSize=%0.2f minHeight=%0.2f maxHeight=%0.2f spawnHeight=%0.2f",
      label ? label : "unspecified",
      terrain->getId(),
      terrain->isServerObject() ? 1 : 0,
      terrain->isClientObject() ? 1 : 0,
      terrain->getTerrainAssetId(),
      terrain->getTerrainFile(),
      terrain->getBlockSize(),
      terrain->getSquareSize(),
      minHeight,
      maxHeight,
      spawnHeight);
   logTerrainMaterialState(label, terrain);

   Forest* forest = nullptr;
   if (Sim::findObject(ForestName, forest) && forest && forest->getData())
   {
      Vector<ForestItem> items;
      forest->getData()->getItems(&items);
      const char* dataFile = forest->getDataField(StringTable->insert("dataFile"), nullptr);
      Con::printf("HSPFoundation: diagnostics[%s] forestId=%u isServer=%u isClient=%u dataFile='%s' itemCount=%u",
         label ? label : "unspecified",
         forest->getId(),
         forest->isServerObject() ? 1 : 0,
         forest->isClientObject() ? 1 : 0,
         dataFile ? dataFile : "",
         items.size());
   }
   else
   {
      Con::warnf("HSPFoundation: diagnostics[%s] forest object is missing or has no data", label ? label : "unspecified");
   }

   SceneObject* stream = nullptr;
   if (Sim::findObject(StreamName, stream))
   {
      Con::printf("HSPFoundation: diagnostics[%s] streamId=%u isServer=%u isClient=%u position='%0.2f %0.2f %0.2f'",
         label ? label : "unspecified",
         stream->getId(),
         stream->isServerObject() ? 1 : 0,
         stream->isClientObject() ? 1 : 0,
         stream->getPosition().x,
         stream->getPosition().y,
         stream->getPosition().z);
   }
   else
   {
      Con::warnf("HSPFoundation: diagnostics[%s] stream object is missing", label ? label : "unspecified");
   }

   return true;
}
