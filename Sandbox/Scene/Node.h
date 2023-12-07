#pragma once

class Camera;
class Mesh;

class Node
{
public:
  Node();
  ~Node();

  Node&   AddChildNode  ( eastl::unique_ptr< Node   >&& node );
  Camera& AddChildCamera( eastl::unique_ptr< Camera >&& camera );
  void    AddChildMesh  ( int meshIndex );
  void    AddChildLight ( int lightIndex );

  void SetTransform( FXMMATRIX transform );
  void SetName( const char* name );

  const char* GetName() const;

  XMMATRIX GetTransform() const;
  XMMATRIX GetParentFullTransform() const;
  XMMATRIX GetFullTransform() const;

  bool IsRootChild() const;

  template< typename NodeFunc >
  void ForEachNode( NodeFunc&& func );
  template< typename CameraFunc >
  void ForEachCamera( CameraFunc&& func );
  template< typename MeshFunc >
  void ForEachMesh( MeshFunc&& func );
  template< typename LightFunc >
  void ForEachLight( LightFunc&& func );

private:
  eastl::string name;

  XMFLOAT4X4 transform;

  Node*                                    parent = nullptr;
  eastl::vector< eastl::unique_ptr< Node   > > children;
  eastl::vector< eastl::unique_ptr< Camera > > cameras;
  eastl::vector< int >                       meshes;
  eastl::vector< int >                       lights;
};

template< typename NodeFunc >
inline void Node::ForEachNode( NodeFunc&& func )
{
  for ( auto& child : children )
    if ( !func( *child ) )
      return;
}

template< typename CameraFunc >
inline void Node::ForEachCamera( CameraFunc&& func )
{
  for ( auto& camera : cameras )
    if ( !func( *camera ) )
      return;
}

template< typename MeshFunc >
inline void Node::ForEachMesh( MeshFunc&& func )
{
  for ( auto& mesh : meshes )
    if ( !func( mesh ) )
      return;
}

template< typename LightFunc >
inline void Node::ForEachLight( LightFunc&& func )
{
  for ( auto& light : lights )
    if ( !func( light ) )
      return;
}
