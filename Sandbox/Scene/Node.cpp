#include "Node.h"
#include "Camera.h"
#include "Render/Mesh.h"

Node::Node()
{
  XMStoreFloat4x4( &this->transform, XMMatrixIdentity() );
}

Node::~Node()
{
}

Node& Node::AddChildNode( eastl::unique_ptr< Node >&& node )
{
  node->parent = this;
  children.push_back( eastl::forward< eastl::unique_ptr< Node > >( node ) );
  return *children.back();
}

Camera& Node::AddChildCamera( eastl::unique_ptr< Camera >&& camera )
{
  cameras.push_back( eastl::forward< eastl::unique_ptr< Camera > >( camera ) );
  return *cameras.back();
}

void Node::AddChildMesh( int meshIndex )
{
  meshes.push_back( meshIndex );
}

void Node::AddChildLight( int lightIndex )
{
  lights.push_back( lightIndex );
}

void Node::SetTransform( FXMMATRIX transform )
{
  return XMStoreFloat4x4( &this->transform, transform );
}

void Node::SetName( const char* name )
{
  this->name = name;
}

const char* Node::GetName() const
{
  return name.data();
}

XMMATRIX Node::GetTransform() const
{
  return XMLoadFloat4x4( &transform );
}

XMMATRIX Node::GetParentFullTransform() const
{
  return parent ? parent->GetFullTransform() : XMMatrixIdentity();
}

XMMATRIX Node::GetFullTransform() const
{
  auto parentTransform = parent ? parent->GetFullTransform() : XMMatrixIdentity();
  auto nodeTransform   = XMLoadFloat4x4( &transform );

  return nodeTransform * parentTransform;
}

bool Node::IsRootChild() const
{
  return parent && !parent->parent;
}
