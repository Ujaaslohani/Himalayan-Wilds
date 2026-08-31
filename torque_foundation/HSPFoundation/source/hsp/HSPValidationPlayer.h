#ifndef _HSP_VALIDATION_PLAYER_H_
#define _HSP_VALIDATION_PLAYER_H_

#include "T3D/player.h"

class HSPValidationPlayer final : public Player
{
   typedef Player Parent;

public:
   DECLARE_CONOBJECT(HSPValidationPlayer);
   DECLARE_CATEGORY("Actor \t Controllable");

   bool onAdd() override;
   void getEyeBaseTransform(MatrixF* mat, bool includeBank) override;
   void getRenderEyeBaseTransform(MatrixF* mat, bool includeBank) override;
   void prepRenderImage(SceneRenderState* state) override;

private:
   void buildValidationEyeTransform(MatrixF* mat, bool renderTransform) const;
};

#endif
