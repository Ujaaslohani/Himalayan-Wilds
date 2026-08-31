#include "hsp/HSPValidationPlayer.h"

#include "T3D/gameBase/gameConnection.h"
#include "console/consoleTypes.h"
#include "math/mMatrix.h"

IMPLEMENT_CO_NETOBJECT_V1(HSPValidationPlayer);

ConsoleDocClass(HSPValidationPlayer,
   "@brief Minimal HSP foundation validation player.\n\n"
   "Uses Torque3D Player movement, gravity, collision, and PlayerData while suppressing "
   "only the locally-controlled validation body render so the first-person camera remains clean.\n\n"
   "@ingroup HSPFoundation\n");

bool HSPValidationPlayer::onAdd()
{
   if (!Parent::onAdd())
      return false;

   mHead.x = -0.28f;
   mDelta.head = mHead;
   mDelta.headVec.set(0.0f, 0.0f, 0.0f);
   return true;
}

void HSPValidationPlayer::buildValidationEyeTransform(MatrixF* mat, bool renderTransform) const
{
   MatrixF pitchMat;
   MatrixF yawMat;
   const Point3F& head = renderTransform ? mDelta.head : mHead;

   pitchMat.set(EulerF(head.x, 0.0f, 0.0f));
   yawMat.set(EulerF(0.0f, 0.0f, head.z));
   MatrixF headMat;
   headMat.mul(yawMat, pitchMat);

   MatrixF base = renderTransform ? getRenderTransform() : getTransform();
   base.setColumn(3, Point3F::Zero);
   mat->mul(base, headMat);

   base = renderTransform ? getRenderTransform() : getTransform();
   Point3F position = base.getPosition();
   position.z += 1.65f;
   mat->setColumn(3, position);
}

void HSPValidationPlayer::getEyeBaseTransform(MatrixF* mat, bool includeBank)
{
   buildValidationEyeTransform(mat, false);
}

void HSPValidationPlayer::getRenderEyeBaseTransform(MatrixF* mat, bool includeBank)
{
   buildValidationEyeTransform(mat, true);
}

void HSPValidationPlayer::prepRenderImage(SceneRenderState* state)
{
   GameConnection* connection = GameConnection::getConnectionToServer();
   if (connection && connection->getControlObject() == this)
      return;

   Parent::prepRenderImage(state);
}
