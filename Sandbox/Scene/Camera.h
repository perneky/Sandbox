#pragma once

class Node;

class Camera
{
public:
  Camera();
  ~Camera();

  void SetProjection( float fovY, float aspect, float nearZ, float farZ );

  XMMATRIX GetProjTransform() const;

  float GetFovY() const;
  float GetAspect() const;
  float GetNearZ() const;
  float GetFarZ() const;

private:
  float fovY;
  float aspect;
  float nearZ;
  float farZ;

  Node* parent = nullptr;
};