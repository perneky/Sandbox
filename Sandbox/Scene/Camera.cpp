#include "Camera.h"
#include "Render/ShaderValues.h"

Camera::Camera()
{
  SetProjection( 90, 1, 1, 100 );
}

XMMATRIX Camera::GetProjTransform() const
{
  #if USE_REVERSE_PROJECTION
    return XMMatrixPerspectiveFovLH( fovY, aspect, farZ, nearZ );
  #else
    return XMMatrixPerspectiveFovLH( fovY, aspect, nearZ, farZ );
  #endif
}

float Camera::GetFovY() const
{
  return fovY;
}

float Camera::GetAspect() const
{
  return aspect;
}

float Camera::GetNearZ() const
{
  return nearZ;
}

float Camera::GetFarZ() const
{
  return farZ;
}

void Camera::SetProjection( float fovY, float aspect, float nearZ, float farZ )
{
  this->fovY   = fovY;
  this->aspect = aspect;
  this->nearZ  = nearZ;
  this->farZ   = farZ;
}

Camera:: ~Camera()
{
}