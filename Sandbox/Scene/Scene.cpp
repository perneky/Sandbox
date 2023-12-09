#include "Scene.h"
#include "Node.h"
#include "Camera.h"
#include "Common/Color.h"
#include "Common/Finally.h"
#include "Common/Files.h"
#include "Render/ShaderStructures.h"
#include "Render/ShaderValues.h"
#include "Render/Utils.h"
#include "Render/RenderManager.h"
#include "Render/Mesh.h"
#include "Render/ResourceDescriptor.h"
#include "Render/ComputeShader.h"
#include "Render/DescriptorHeap.h"
#include "Render/Utils.h"
#include "Render/RTTopLevelAccelerator.h"
#include "Render/Swapchain.h"
#include "Render/RTShaders.h"
#include "Render/Denoiser.h"

#include "assimp/inc/assimp/Importer.hpp"
#include "assimp/inc/assimp/scene.h"
#include "assimp/inc/assimp/postprocess.h"

#pragma comment( lib, "assimp-vc143-mt.lib" )

static constexpr float initialMinLog = -12.0f;
static constexpr float initialMaxLog = 4.0f;

static constexpr int triangleExtractRootConstants = 13;

static void InitializeManualExposure( CommandList& commandList, Resource& expBuffer, Resource& expOnlyBuffer, float exposure )
{
  ExposureBuffer params;
  params.exposure        = exposure;
  params.invExposure     = 1.0f / exposure;
  params.targetExposure  = exposure;
  params.weightedHistAvg = 0;
  params.minLog          = initialMinLog;
  params.maxLog          = initialMaxLog;
  params.logRange        = initialMaxLog - initialMinLog;
  params.invLogRange     = 1.0f / params.logRange;

  auto uploadExposure = RenderManager::GetInstance().GetUploadBufferForResource( expBuffer );
  commandList.UploadBufferResource( eastl::move( uploadExposure ), expBuffer, &params, sizeof( params ) );

  auto uploadExposureOnly = RenderManager::GetInstance().GetUploadBufferForResource( expOnlyBuffer );
  commandList.UploadBufferResource( eastl::move( uploadExposureOnly ), expOnlyBuffer, &exposure, sizeof( exposure ) );
}

static eastl::wstring GetFileName( const wchar_t* hostFolder )
{
  for ( auto& file : std::filesystem::directory_iterator( hostFolder ) )
    if ( file.is_regular_file() && file.path().extension() == ".fbx" )
      return std::filesystem::canonical( file.path() ).wstring().data();

  return L"";
}

static bool Validate( aiMesh* mesh, unsigned meshIx, eastl::wstring& error )
{
  aiString    meshName = mesh->mName;
  const char* meshNameC = meshName.C_Str();
  if ( !mesh->HasPositions() )
  {
    error = L"Mesh (" + eastl::to_wstring( meshIx ) + L") has no positions: " + W( meshNameC );
    return false;
  }
  if ( !mesh->HasFaces() )
  {
    error = L"Mesh (" + eastl::to_wstring( meshIx ) + L") has no faces: " + W( meshNameC );
    return false;
  }
  if ( !mesh->HasNormals() )
  {
    error = L"Mesh (" + eastl::to_wstring( meshIx ) + L") has no normals: " + W( meshNameC );
    return false;
  }
  if ( mesh->HasBones() )
  {
    error = L"Mesh (" + eastl::to_wstring( meshIx ) + L") has bones, which is not yet supported: " + W( meshNameC );
    return false;
  }

  return true;
}

static void Compress( XMHALF4& c, const aiVector3D& u )
{
  Float16::Convert( &c.x, u.x );
  Float16::Convert( &c.y, u.y );
  Float16::Convert( &c.z, u.z );
  Float16::Convert( &c.w, 1.0f );
}

static void Compress( XMHALF2& c, const aiVector3D& u )
{
  Float16::Convert( &c.x, u.x );
  Float16::Convert( &c.y, u.y );
}

struct MeshNode
{
  int        meshIx;
  XMFLOAT4X4 transform;
};

static void WalkDCCNodes( aiNode& dccNode, Node& sceneNode )
{
  sceneNode.SetTransform( XMMatrixTranspose( XMLoadFloat4x4( (XMFLOAT4X4*)&dccNode.mTransformation ) ) );
  sceneNode.SetName( dccNode.mName.C_Str() );

  for ( unsigned meshIx = 0; meshIx < dccNode.mNumMeshes; meshIx++ )
    sceneNode.AddChildMesh( dccNode.mMeshes[ meshIx ] );

  for ( unsigned childIx = 0; childIx < dccNode.mNumChildren; childIx++ )
  {
    auto& childNode = sceneNode.AddChildNode( eastl::make_unique< Node >() );
    WalkDCCNodes( *dccNode.mChildren[ childIx ], childNode );
  }
}

void Scene::MarshallSceneToGPU( Node& sceneNode, int& instanceCount, Scene& scene )
{
  auto nodeSlot = int( scene.nodeSlots.size() );

  if ( sceneNode.IsRootChild() )
    scene.rootNodeChildrenIndices.emplace_back( nodeSlot );

  scene.nodeToIndex[ &sceneNode ] = nodeSlot;
  scene.indexToNode[ nodeSlot   ] = &sceneNode;

  scene.nodeSlots.emplace_back();

  XMStoreFloat4x4( &scene.nodeSlots[ nodeSlot ].worldTransform, sceneNode.GetTransform() );

  scene.nodeSlots[ nodeSlot ].cameraSlot      = InvalidSlot;
  scene.nodeSlots[ nodeSlot ].lightSlot       = InvalidSlot;
  scene.nodeSlots[ nodeSlot ].firstMeshSlot   = InvalidSlot;
  scene.nodeSlots[ nodeSlot ].firstChildSlot  = InvalidSlot;
  scene.nodeSlots[ nodeSlot ].nextSiblingSlot = InvalidSlot;

  // From assimp, there can be only one camera per node. So we can just take the first one.
  sceneNode.ForEachCamera( [&]( Camera& camera ) mutable
  {
    scene.nodeSlots[ nodeSlot ].cameraSlot = int( scene.cameraSlots.size() );

    scene.cameraSlots.emplace_back();
    auto& cameraSlot = scene.cameraSlots.back();

    XMStoreFloat4x4( &cameraSlot.projTransform, camera.GetProjTransform() );

    cameraNodeIndex = nodeSlot;

    return false;
  });

  // From assimp, there can be only one light per node. So we can just take the first one.
  sceneNode.ForEachLight( [&]( int lightIndex ) mutable
  {
    scene.nodeSlots[ nodeSlot ].lightSlot = lightIndex;
    return false;
  });

  int lastMeshSlot = -1;
  sceneNode.ForEachMesh( [&]( int meshIndex ) mutable
  {
    instanceCount++;

    int meshSlotIndex = int( scene.meshSlots.size() );
      
    if ( scene.nodeSlots[ nodeSlot ].firstMeshSlot == InvalidSlot )
      scene.nodeSlots[ nodeSlot ].firstMeshSlot = meshSlotIndex;
    else
      scene.meshSlots[ lastMeshSlot ].nextSlotIndex = meshSlotIndex;

    lastMeshSlot = meshSlotIndex;

    scene.meshSlots.emplace_back();
    auto& meshSlot = scene.meshSlots.back();

    auto& aabb = scene.meshes[ meshIndex ]->GetAABB();

    meshSlot.aabbCenter     = XMFLOAT4( aabb.Center.x,  aabb.Center.y,  aabb.Center.z,  1 );
    meshSlot.aabbExtents    = XMFLOAT4( aabb.Extents.x, aabb.Extents.y, aabb.Extents.z, 1 );
    meshSlot.ibIndex        = scene.meshes[ meshIndex ]->GetIndexBufferSlot() - SceneBufferResourceBaseSlot;
    meshSlot.vbIndex        = scene.meshes[ meshIndex ]->GetVertexBufferSlot() - SceneBufferResourceBaseSlot;
    meshSlot.indexCount     = scene.meshes[ meshIndex ]->GetIndexCount();
    meshSlot.materialIndex  = scene.meshes[ meshIndex ]->GetMaterialIndex();
    meshSlot.randomValues.x = PackedVector::XMConvertFloatToHalf( Random() );
    meshSlot.randomValues.y = PackedVector::XMConvertFloatToHalf( Random() );
    meshSlot.randomValues.z = PackedVector::XMConvertFloatToHalf( Random() );
    meshSlot.randomValues.w = PackedVector::XMConvertFloatToHalf( Random() );
    meshSlot.nextSlotIndex  = InvalidSlot;

    return true;
  });

  int lastChildSlot = -1;
  sceneNode.ForEachNode( [&]( Node& childNode ) mutable
  {
    int childNodeSlotIndex = int( scene.nodeSlots.size() );

    MarshallSceneToGPU( childNode, instanceCount, scene );

    if ( scene.nodeSlots[ nodeSlot ].firstChildSlot == InvalidSlot )
      scene.nodeSlots[ nodeSlot ].firstChildSlot = childNodeSlotIndex;
    else
      scene.nodeSlots[ lastChildSlot ].nextSiblingSlot = childNodeSlotIndex;

    lastChildSlot = childNodeSlotIndex;

    return true;
  });
}

void Scene::MarshallSceneToRTInstances( Node& sceneNode, eastl::vector< RTInstance >& rtInstances, Scene& scene )
{
  auto nodeTransform = sceneNode.GetFullTransform();

  sceneNode.ForEachMesh( [&]( int meshIndex ) mutable
  {
    rtInstances.emplace_back();
    auto& instance = rtInstances.back();

    instance.accel = &scene.meshes[ meshIndex ]->GetRTBottomLevelAccelerator();
    XMStoreFloat4x4( &instance.transform, nodeTransform );

    return true;
  });

  sceneNode.ForEachNode( [&]( Node& childNode ) mutable
  {
    MarshallSceneToRTInstances( childNode, rtInstances, scene );
    return true;
  });
}

static void NotifyCamerasOnWindowSizeChange( Node& sceneNode, float aspect )
{
  sceneNode.ForEachCamera( [&]( Camera& camera ) mutable
  {
    camera.SetProjection( camera.GetFovY(), aspect, camera.GetNearZ(), camera.GetFarZ() );

    return false;
  });

  int lastChildSlot = -1;
  sceneNode.ForEachNode( [&]( Node& childNode ) mutable
  {
    NotifyCamerasOnWindowSizeChange( childNode, aspect );

    return true;
  });
}

aiNode* FindNodeByName( aiNode* node, const aiString& name )
{
  if ( node->mName == name )
    return node;
  
  for ( unsigned childIx = 0; childIx < node->mNumChildren; childIx++ )
  {
    auto found = FindNodeByName( node->mChildren[ childIx ], name );
    if ( found )
      return found;
  }
  return nullptr;
}

XMMATRIX CalcNodeTransform( aiNode* node )
{
  auto parentTransform = node->mParent ? CalcNodeTransform( node->mParent ) : XMMatrixIdentity();
  auto nodeTransform   = XMMatrixTranspose( XMLoadFloat4x4( (XMFLOAT4X4*)&node->mTransformation ) );

  return nodeTransform * parentTransform;
}

static bool LoadTexture( CommandList& commandList, const wchar_t* hostFolder, aiMaterial* material, aiTextureType textureType, int& texutreId, int* refTexutreId = nullptr )
{
  aiString texturePath;
  if ( material->GetTexture( textureType, 0, &texturePath ) == aiReturn_SUCCESS )
  {
    eastl::string ddsTexturePath( texturePath.C_Str() );
    ddsTexturePath.resize( ddsTexturePath.size() - 3 );
    ddsTexturePath += "dds";

    texutreId = RenderManager::GetInstance().Get2DTexture( CommandQueueType::Direct, commandList, eastl::wstring( hostFolder ) + L"/" + W( ddsTexturePath.data() ), refTexutreId );
    return texutreId > -1;
  }

  return false;
}

Scene::~Scene()
{
}

void Scene::SetManualExposure( float exposure )
{
  manualExposure = exposure;
}

void Scene::TearDown( CommandList* commandList )
{
  if ( upscaling )
  {
    upscaling->TearDown();
    upscaling.reset();
  }

  if ( denoiser )
    denoiser->TearDown( commandList );
}

Scene::Scene( CommandList& commandList, const wchar_t* hostFolder, int screenWidth, int screenHeight )
: manualExposure( 2.0f )
, targetLuminance( 0.6f )
, adaptationRate( 0.093f )
, minExposure( 0.35f )
, maxExposure( 3.00f )
, bloomThreshold( 2.0f )
, bloomStrength( 0.1f )
{
  auto& manager = RenderManager::GetInstance();
  auto& device  = manager.GetDevice();

  auto sceneFilePath = GetFileName( hostFolder );
  if ( sceneFilePath.empty() )
  {
    error = L"Failed to find file in: ";
    error += hostFolder;
    return;
  }

  Assimp::Importer importer;

  unsigned flags = aiProcess_OptimizeMeshes
                 | aiProcess_MakeLeftHanded
                 | aiProcess_FlipUVs
                 | aiProcess_FlipWindingOrder
                 | aiProcess_GenNormals
                 | aiProcess_JoinIdenticalVertices
                 | aiProcess_ImproveCacheLocality
                 | aiProcess_LimitBoneWeights
                 | aiProcess_RemoveRedundantMaterials
                 | aiProcess_Triangulate
                 | aiProcess_SortByPType
                 | aiProcess_FindDegenerates
                 | aiProcess_FindInvalidData
                 | aiProcess_FindInstances
                 | aiProcess_ValidateDataStructure
                 | aiProcess_CalcTangentSpace
                 | aiProcess_SplitLargeMeshes;

  importer.SetPropertyInteger( AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 0xFFFF / 3 );

  importer.ReadFile( N( sceneFilePath.data() ).data(), flags );

  const aiScene* scene = importer.GetScene();

  if ( !scene )
  {
    error  = L"Failed to import file: ";
    error += sceneFilePath;
    error += L" - ";
    error += W( importer.GetErrorString() );
    error += L"\n";
    return;
  }

  if ( !scene->HasMeshes() )
  {
    error  = L"file has no meshes: ";
    error += sceneFilePath;
    return;
  }    

  for ( unsigned meshIx = 0; meshIx < scene->mNumMeshes; meshIx++ )
  {
    aiMesh* mesh = scene->mMeshes[ meshIx ];
    if ( !Validate( mesh, meshIx, error ) )
      return;
  }

  eastl::vector< MaterialSlot > materialSlots;
  for ( unsigned materialIx = 0; materialIx < scene->mNumMaterials; materialIx++ )
  {
    aiMaterial* material = scene->mMaterials[ materialIx ];
    materialSlots.emplace_back();
    auto& materialSlot = materialSlots.back();

    auto name = material->GetName();

    aiColor3D baseColor( 1 );
    material->Get( AI_MATKEY_COLOR_DIFFUSE, baseColor );

    materialSlot.albedo.x = PackedVector::XMConvertFloatToHalf( pow( baseColor.r, 1.0f / 2.2f ) );
    materialSlot.albedo.y = PackedVector::XMConvertFloatToHalf( pow( baseColor.g, 1.0f / 2.2f ) );
    materialSlot.albedo.z = PackedVector::XMConvertFloatToHalf( pow( baseColor.b, 1.0f / 2.2f ) );

    aiColor3D emissive( 0 );
    material->Get( AI_MATKEY_COLOR_EMISSIVE, emissive );

    materialSlot.emissive.x = PackedVector::XMConvertFloatToHalf( pow( emissive.r, 1.0f / 2.2f ) );
    materialSlot.emissive.y = PackedVector::XMConvertFloatToHalf( pow( emissive.g, 1.0f / 2.2f ) );
    materialSlot.emissive.z = PackedVector::XMConvertFloatToHalf( pow( emissive.b, 1.0f / 2.2f ) );

    bool isTwoSided    = false;
    bool isAlphaTested = false;
    bool isTranslucent = false;
    bool isFlipWinding = false;

    float roughness = 1;
    float metallic  = 0;
    float alpha     = 1;

    // There is AI_MATKEY_METALLIC_FACTOR, but only for 'Maya|metallic'
    material->Get( AI_MATKEY_ROUGHNESS_FACTOR, roughness );
    material->Get( AI_MATKEY_REFLECTIVITY, metallic );
    material->Get( AI_MATKEY_OPACITY, alpha );
    material->Get( "$raw.TwoSided", 0, 0, isTwoSided );
    material->Get( "$raw.AlphaTested", 0, 0, isAlphaTested );
    material->Get( "$raw.Translucent", 0, 0, isTranslucent );
    material->Get( "$raw.FlipWinding", 0, 0, isFlipWinding );

    materialSlot.albedo.w = PackedVector::XMConvertFloatToHalf( alpha );

    materialSlot.roughness_metallic.x = PackedVector::XMConvertFloatToHalf( roughness );
    materialSlot.roughness_metallic.y = PackedVector::XMConvertFloatToHalf( metallic );

    if ( isTwoSided )
      materialSlot.flags |= MaterialSlot::TwoSided;
    if ( isAlphaTested )
      materialSlot.flags |= MaterialSlot::AlphaTested;
    if ( isTranslucent )
      materialSlot.flags |= MaterialSlot::Translucent;
    if ( isFlipWinding )
      materialSlot.flags |= MaterialSlot::FlipWinding;

    materialSlot.albedoTextureIndex = -1;
    materialSlot.albedoTextureRefIndex = -1;
    materialSlot.normalTextureIndex = -1;
    materialSlot.roughnessTextureIndex = -1;
    materialSlot.metallicTextureIndex = -1;

    LoadTexture( commandList, hostFolder, material, aiTextureType_DIFFUSE,   materialSlot.albedoTextureIndex, &materialSlot.albedoTextureRefIndex );
    LoadTexture( commandList, hostFolder, material, aiTextureType_SHININESS, materialSlot.roughnessTextureIndex );
    LoadTexture( commandList, hostFolder, material, aiTextureType_NORMALS,   materialSlot.normalTextureIndex );
    LoadTexture( commandList, hostFolder, material, aiTextureType_METALNESS, materialSlot.metallicTextureIndex );
  }

  materialBuffer = CreateBufferFromData( materialSlots.data(), int( materialSlots.size() ), ResourceType::Buffer, device, commandList, L"materialBuffer" );
  auto materialBufferDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView, MaterialBufferSlot, *materialBuffer, sizeof( MaterialSlot ) );
  materialBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( materialBufferDesc ) );
  commandList.ChangeResourceState( { { *materialBuffer, ResourceStateBits::NonPixelShaderInput | ResourceStateBits::PixelShaderInput } } );

  modelMetaBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, sizeof( ModelMetaSlot ) * scene->mNumMeshes, sizeof( ModelMetaSlot ), L"modelMetaBuffer" );
  auto modelMetaBufferDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView, ModelMetaBufferSlot, *modelMetaBuffer, sizeof( ModelMetaSlot ) );
  modelMetaBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( modelMetaBufferDesc ) );

  for ( unsigned meshIx = 0; meshIx < scene->mNumMeshes; meshIx++ )
  {
    aiMesh* mesh = scene->mMeshes[ meshIx ];

    eastl::vector< VertexFormat > vertices;
    eastl::vector< uint32_t >     indices;

    auto vMin = XMLoadFloat3( (XMFLOAT3*)&mesh->mVertices[ 0 ].x );
    auto vMax = XMLoadFloat3( (XMFLOAT3*)&mesh->mVertices[ 0 ].x );

    for ( unsigned vtxIx = 0; vtxIx < mesh->mNumVertices; vtxIx++ )
    {
      vertices.emplace_back();
      auto& vtx = vertices.back();

      aiVector3D v  = mesh->mVertices[ vtxIx ];
      aiVector3D t  = mesh->mTangents ? mesh->mTangents[ vtxIx ] : aiVector3D();
      aiVector3D b  = mesh->mBitangents ? mesh->mBitangents[ vtxIx ] : aiVector3D();
      aiVector3D n  = mesh->mNormals[ vtxIx ];
      aiVector3D tc = mesh->HasTextureCoords( 0 ) ? mesh->mTextureCoords[ 0 ][ vtxIx ] : aiVector3D(0);

      t.Normalize();
      b.Normalize();
      n.Normalize();
      
      Compress( vtx.position, v );
      Compress( vtx.tangent, t );
      Compress( vtx.bitangent, b );
      Compress( vtx.normal, n );
      Compress( vtx.texcoord, tc );

      auto vPoint = XMLoadFloat3( (XMFLOAT3*)&v.x );
      vMin = XMVectorMin( vMin, vPoint );
      vMax = XMVectorMax( vMax, vPoint );
    }

    BoundingBox aabb;
    BoundingBox::CreateFromPoints( aabb, vMin, vMax );

    for ( unsigned faceIx = 0; faceIx < mesh->mNumFaces; faceIx++ )
    {
      auto& face = mesh->mFaces[ faceIx ];

      assert( face.mNumIndices == 3 );

      indices.emplace_back( uint32_t( face.mIndices[ 0 ] ) );
      indices.emplace_back( uint32_t( face.mIndices[ 1 ] ) );
      indices.emplace_back( uint32_t( face.mIndices[ 2 ] ) );
    }

    auto debugVBName = W( mesh->mName.C_Str() ) + L"_VB";
    auto debugIBName = W( mesh->mName.C_Str() ) + L"_IB";
    auto vbGPU = CreateBufferFromData( vertices.data(), int( vertices.size() ), ResourceType::Buffer, device, commandList, debugVBName.data() );
    auto ibGPU = CreateBufferFromData( indices .data(), int( indices .size() ), ResourceType::Buffer, device, commandList, debugIBName.data() );

    commandList.ChangeResourceState( { { *vbGPU, ResourceStateBits::NonPixelShaderInput }
                                     , { *ibGPU, ResourceStateBits::NonPixelShaderInput } } );

    bool isOpaque = !( materialSlots[ mesh->mMaterialIndex ].flags & MaterialSlot::AlphaTested )
                 && !( materialSlots[ mesh->mMaterialIndex ].flags & MaterialSlot::Translucent );
    meshes.emplace_back( eastl::make_unique< Mesh >( commandList
                                                 , eastl::move( vbGPU )
                                                 , eastl::move( ibGPU )
                                                 , int( vertices.size() )
                                                 , int( indices.size() )
                                                 , mesh->mMaterialIndex
                                                 , isOpaque
                                                 , *modelMetaBuffer
                                                 , meshIx
                                                 , aabb
                                                 , mesh->mName.C_Str() ) );
  }

  rootNode = eastl::make_unique< Node >();

  WalkDCCNodes( *scene->mRootNode, *rootNode );

  for ( unsigned lightIx = 0; lightIx < scene->mNumLights; lightIx++ )
  {
    aiLight* light = scene->mLights[ lightIx ];

    if ( light->mType != aiLightSource_DIRECTIONAL )
      continue;

    auto lightNode = FindNodeByName( light->mName.C_Str() );
    assert( lightNode );

    lightNode->AddChildLight( int( lightSlots.size() ) );

    lightSlots.emplace_back();
    auto& lightSlot = lightSlots.back();

    assert( light->mType == aiLightSource_DIRECTIONAL || light->mType == aiLightSource_SPOT || light->mType == aiLightSource_POINT );

    lightSlot.color.x       = PackedVector::XMConvertFloatToHalf( light->mColorDiffuse.r );
    lightSlot.color.y       = PackedVector::XMConvertFloatToHalf( light->mColorDiffuse.g );
    lightSlot.color.z       = PackedVector::XMConvertFloatToHalf( light->mColorDiffuse.b );
    lightSlot.color.w       = 1;
    lightSlot.attenuation.x = PackedVector::XMConvertFloatToHalf( light->mAttenuationConstant );
    lightSlot.attenuation.y = PackedVector::XMConvertFloatToHalf( light->mAttenuationLinear );
    lightSlot.attenuation.z = PackedVector::XMConvertFloatToHalf( light->mAttenuationQuadratic );
    lightSlot.theta_phi.x   = PackedVector::XMConvertFloatToHalf( light->mAngleInnerCone );
    lightSlot.theta_phi.y   = PackedVector::XMConvertFloatToHalf( light->mAngleOuterCone );
    lightSlot.castShadow    = light->mType == aiLightSource_DIRECTIONAL;
    lightSlot.scatterShadow = 0;
    lightSlot.type          = light->mType == aiLightSource_DIRECTIONAL ? LightType::Directional : ( light->mType == aiLightSource_POINT ? LightType::Point : LightType::Spot );

    if ( light->mType == aiLightSource_DIRECTIONAL )
    {
      lightSlot.attenuation.x = PackedVector::XMConvertFloatToHalf( 1 );
      lightSlot.attenuation.y = PackedVector::XMConvertFloatToHalf( 0 );
      lightSlot.attenuation.z = PackedVector::XMConvertFloatToHalf( 0 );
    }
  }

  lightBuffer = CreateBufferFromData( lightSlots.data(), int( lightSlots.size() ), ResourceType::Buffer, device, commandList, L"lightBuffer" );
  auto lightBufferDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView, LightBufferSlot, *lightBuffer, sizeof( LightSlot ) );
  lightBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( lightBufferDesc ) );
  commandList.ChangeResourceState( { { *lightBuffer, ResourceStateBits::NonPixelShaderInput } } );

  if ( scene->HasCameras() )
  {
    for ( unsigned cameraIx = 0; cameraIx < scene->mNumCameras; cameraIx++ )
    {
      auto& dccCamera = scene->mCameras[ cameraIx ];

      auto cameraNode = FindNodeByName( dccCamera->mName.C_Str() );
      assert( cameraNode );

      auto cameraForward      = XMLoadFloat3( (XMFLOAT3*)&dccCamera->mLookAt );
      auto cameraUp           = XMLoadFloat3( (XMFLOAT3*)&dccCamera->mUp );
      auto cameraPosition     = XMLoadFloat3( (XMFLOAT3*)&dccCamera->mPosition );
      auto cameraView         = XMMatrixLookToLH( cameraPosition, cameraForward, cameraUp );
      auto cameraTransform    = XMMatrixInverse( nullptr, cameraView );
      auto cameraFOVY         = (dccCamera->mHorizontalFOV * 2) / dccCamera->mAspect;
      auto cameraNearDistance = dccCamera->mClipPlaneNear;
      auto cameraFarDistance  = dccCamera->mClipPlaneFar;

      eastl::unique_ptr< Camera > camera = eastl::make_unique< Camera >();
      camera->SetProjection( cameraFOVY, float( screenWidth ) / screenHeight, cameraNearDistance, cameraFarDistance);

      cameraNode->SetTransform( cameraTransform * cameraNode->GetTransform() );
      cameraNode->AddChildCamera( eastl::move( camera ) );
    }
  }

  BuildSceneBuffers( commandList );

  eastl::vector< RTInstance > rtInstances;
  MarshallSceneToRTInstances( *rootNode, rtInstances, *this );
  tlas = device.CreateRTTopLevelAccelerator( commandList, rtInstances, RTSceneSlot );

  indirectDrawCountBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, sizeof( uint32_t ) * 9, sizeof( uint32_t ), L"indirectDrawCountBuffer" );
  auto indirectDrawCountBufferDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, IndirectDrawCountBufferSlot, *indirectDrawCountBuffer, sizeof( uint32_t ) );
  indirectDrawCountBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( indirectDrawCountBufferDesc ) );

  frameParamsBuffer = device.CreateBuffer( ResourceType::ConstantBuffer, HeapType::Default, true, sizeof( FrameParams ), sizeof( FrameParams ), L"frameParamsBuffer" );
  auto frameParamsBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, FrameParamsBufferUAVSlot, *frameParamsBuffer, sizeof( FrameParams ) );
  auto frameParamsBufferCBVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ConstantBufferView, FrameParamsBufferCBVSlot, *frameParamsBuffer, sizeof( FrameParams ) );
  frameParamsBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( frameParamsBufferUAVDesc ) );
  frameParamsBuffer->AttachResourceDescriptor( ResourceDescriptorType::ConstantBufferView, eastl::move( frameParamsBufferCBVDesc ) );

  lightParamsBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, sizeof( LightParams ), int( sizeof( LightParams ) * lightSlots.size() ), L"lightParamsBuffer" );
  auto lightParamsBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, ProcessedLightBufferUAVSlot, *lightParamsBuffer, sizeof( LightParams ) );
  auto lightParamsBufferSRVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView, ProcessedLightBufferSRVSlot, *lightParamsBuffer, sizeof( LightParams ) );
  lightParamsBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( lightParamsBufferUAVDesc ) );
  lightParamsBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( lightParamsBufferSRVDesc ) );

  skyBuffer = device.CreateBuffer( ResourceType::ConstantBuffer, HeapType::Default, true, sizeof( LightParams ), int( sizeof( LightParams ) * lightSlots.size() ), L"skyBuffer" );
  auto skyBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, SkyBufferUAVSlot, *skyBuffer, sizeof( LightParams ) );
  auto skyBufferCBVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ConstantBufferView, SkyBufferCBVSlot, *skyBuffer, sizeof( LightParams ) );
  skyBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( skyBufferUAVDesc ) );
  skyBuffer->AttachResourceDescriptor( ResourceDescriptorType::ConstantBufferView, eastl::move( skyBufferCBVDesc ) );

  skyTexture = device.CreateCubeTexture( commandList, 512, nullptr, 0, RenderManager::HDRFormat, true, SkyTextureSlot, eastl::nullopt, false, L"skyTexture" );

  exposureBuffer = device.CreateBuffer( ResourceType::ConstantBuffer, HeapType::Default, true, sizeof( ExposureBuffer ), sizeof( ExposureBuffer ), L"Exposure" );
  auto exposureBufferCBVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ConstantBufferView,  ExposureBufferCBVSlot, *exposureBuffer, sizeof( ExposureBuffer ) );
  auto exposureBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, ExposureBufferUAVSlot, *exposureBuffer, sizeof( ExposureBuffer ) );
  exposureBuffer->AttachResourceDescriptor( ResourceDescriptorType::ConstantBufferView,  eastl::move( exposureBufferCBVDesc ) );
  exposureBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( exposureBufferUAVDesc ) );

  exposureOnlyBuffer = device.Create2DTexture( commandList, 1, 1, nullptr, 0, PixelFormat::R32F, false, ExposureOnlySlot, ExposureOnlyUAVSlot, false, L"ExposureOnly");

  histogramBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, 256 * sizeof( uint32_t ), sizeof( uint32_t ), L"Histogram" );
  auto histogramBufferDesc    = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView,  HistogramBufferSlot,    *histogramBuffer, sizeof( uint32_t ) );
  auto histogramBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, HistogramBufferUAVSlot, *histogramBuffer, sizeof( uint32_t ) );
  histogramBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView,  eastl::move( histogramBufferDesc    ) );
  histogramBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( histogramBufferUAVDesc ) );

  InitializeManualExposure( commandList, *exposureBuffer, *exposureOnlyBuffer, 1 );

  auto cullingFile            = ReadFileToMemory( L"Content/Shaders/Culling.cso" );
  auto prepareCullingFile     = ReadFileToMemory( L"Content/Shaders/PrepareCulling.cso" );
  auto specBRDFLUTFile        = ReadFileToMemory( L"Content/Shaders/SpecBRDFLUT.cso" );
  auto blurFile               = ReadFileToMemory( L"Content/Shaders/Blur.cso" );
  auto downsampleFile         = ReadFileToMemory( L"Content/Shaders/Downsample.cso" );
  auto downsample4File        = ReadFileToMemory( L"Content/Shaders/Downsample4.cso" );
  auto downsampleMSAA4File    = ReadFileToMemory( L"Content/Shaders/DownsampleMSAA4.cso" );
  auto downsample4WLumaFile   = ReadFileToMemory( L"Content/Shaders/Downsample4WithLuminanceFilter.cso" );
  auto processReflectionFile  = ReadFileToMemory( L"Content/Shaders/ProcessReflection.cso" );
  auto extractBloomFile       = ReadFileToMemory( L"Content/Shaders/ExtractBloom.cso" );
  auto blurBloomFile          = ReadFileToMemory( L"Content/Shaders/BlurBloom.cso" );
  auto downsampleBloomFile    = ReadFileToMemory( L"Content/Shaders/DownsampleBloom.cso" );
  auto upsampleBlurBloomFile  = ReadFileToMemory( L"Content/Shaders/UpsampleAndBlurBloom.cso" );
  auto generateHistogramFile  = ReadFileToMemory( L"Content/Shaders/GenerateHistogram.cso" );
  auto adaptExposureFile      = ReadFileToMemory( L"Content/Shaders/AdaptExposure.cso" );
  auto traceShadowFile        = ReadFileToMemory( L"Content/Shaders/TraceShadow.cso" );
  auto traceShadow_sigFile    = ReadFileToMemory( L"Content/Shaders/TraceShadow_sig.cso" );
  auto traceGIFile            = ReadFileToMemory( L"Content/Shaders/TraceGI.cso" );
  auto traceGI_sigFile        = ReadFileToMemory( L"Content/Shaders/TraceGI_sig.cso" );
  auto traceAOFile            = ReadFileToMemory( L"Content/Shaders/TraceAmbientOcclusion.cso" );
  auto traceAO_sigFile        = ReadFileToMemory( L"Content/Shaders/TraceAmbientOcclusion_sig.cso" );
  auto traceReflecionFile     = ReadFileToMemory( L"Content/Shaders/TraceReflection.cso" );
  auto traceReflecion_sigFile = ReadFileToMemory( L"Content/Shaders/TraceReflection_sig.cso" );

  cullingShader            = device.CreateComputeShader( cullingFile.data(), int( cullingFile.size() ), L"Culling" );
  prepareCullingShader     = device.CreateComputeShader( prepareCullingFile.data(), int( prepareCullingFile.size() ), L"PrepareCulling" );
  specBRDFLUTShader        = device.CreateComputeShader( specBRDFLUTFile.data(), int( specBRDFLUTFile.size() ), L"SpecBRDFLUT" );
  blurShader               = device.CreateComputeShader( blurFile.data(), int( blurFile.size() ), L"Blur" );
  downsampleShader         = device.CreateComputeShader( downsampleFile.data(), int( downsampleFile.size() ), L"Downsample" );
  downsample4Shader        = device.CreateComputeShader( downsample4File.data(), int( downsample4File.size() ), L"Downsample4" );
  downsampleMSAA4Shader    = device.CreateComputeShader( downsampleMSAA4File.data(), int( downsampleMSAA4File.size() ), L"DownsampleMSAA4" );
  downsample4WLumaShader   = device.CreateComputeShader( downsample4WLumaFile.data(), int( downsample4WLumaFile.size() ), L"Downsample4WLuma" );
  downsampleBloomShader    = device.CreateComputeShader( downsampleBloomFile.data(), int( downsampleBloomFile.size() ), L"DownsampleBloom" );
  upsampleBlurBloomShader  = device.CreateComputeShader( upsampleBlurBloomFile.data(), int( upsampleBlurBloomFile.size() ), L"UpsampleBlurBloom" );
  extractBloomShader       = device.CreateComputeShader( extractBloomFile.data(), int( extractBloomFile.size() ), L"ExtractBloom" );
  blurBloomShader          = device.CreateComputeShader( blurBloomFile.data(), int( blurBloomFile.size() ), L"BlurBloom" );
  generateHistogramShader  = device.CreateComputeShader( generateHistogramFile.data(), int( generateHistogramFile.size() ), L"GenerateHistogram" );
  adaptExposureShader      = device.CreateComputeShader( adaptExposureFile.data(), int( adaptExposureFile.size() ), L"AdaptExposure" );

  traceAOShader = device.CreateRTShaders( commandList
                                        , traceAO_sigFile
                                        , traceAOFile
                                        , L"raygen"
                                        , L"miss"
                                        , L"anyHit"
                                        , L"closestHit"
                                        , sizeof( XMFLOAT2 ) // BuiltInTriangleIntersectionAttributes
                                        , sizeof( AOPayload )
                                        , 1 );

  traceShadowShader = device.CreateRTShaders( commandList
                                            , traceShadow_sigFile
                                            , traceShadowFile
                                            , L"raygen"
                                            , L"miss"
                                            , L"anyHit"
                                            , L"closestHit"
                                            , sizeof( XMFLOAT2 ) // BuiltInTriangleIntersectionAttributes
                                            , sizeof( ShadowPayload )
                                            , 1 );

  traceGIShader = device.CreateRTShaders( commandList
                                        , traceGI_sigFile
                                        , traceGIFile
                                        , L"raygen"
                                        , L"miss"
                                        , L"anyHit"
                                        , L"closestHit"
                                        , sizeof( XMFLOAT2 ) // BuiltInTriangleIntersectionAttributes
                                        , sizeof( GIPayload )
                                        , GI_MAX_ITERATIONS );

  traceReflectionShader = device.CreateRTShaders( commandList
                                                , traceReflecion_sigFile
                                                , traceReflecionFile
                                                , L"raygen"
                                                , L"miss"
                                                , L"anyHit"
                                                , L"closestHit"
                                                , sizeof( XMFLOAT2 ) // BuiltInTriangleIntersectionAttributes
                                                , sizeof( ReflectionPayload )
                                                , 1 );

  CreateBRDFLUTTexture( commandList );

  auto scramblingRankingTextureData = ReadFileToMemory( L"Content/EngineTextures/scrambling_ranking_128x128_2d_1spp.dds" );
  auto sobolTextureData             = ReadFileToMemory( L"Content/EngineTextures/sobol_256_4d.dds" );

  int width, height;
  PixelFormat pf;

  auto texels = ParseSimpleDDS( scramblingRankingTextureData, width, height, pf );
  assert( pf == PixelFormat::RGBA8888UN );
  pf = PixelFormat::RGBA8888U;
  scramblingRankingTexture = device.Create2DTexture( commandList, width, height, texels.first, texels.second, pf, false, ScramblingRankingSlot, eastl::nullopt, false, L"Scrambling ranking" );

  texels = ParseSimpleDDS( sobolTextureData, width, height, pf );
  assert( pf == PixelFormat::RGBA8888UN );
  pf = PixelFormat::RGBA8888U;
  sobolTexture = device.Create2DTexture( commandList, width, height, texels.first, texels.second, pf, false, SobolSlot, eastl::nullopt, false, L"Sobol" );

  RecreateScrenSizeDependantTextures( commandList, screenWidth, screenHeight );
}

const eastl::wstring& Scene::GetError() const
{
  return error;
}

void Scene::OnScreenResize( CommandList& commandList, int width, int height )
{
  NotifyCamerasOnWindowSizeChange( *rootNode, float( width ) / height );
  BuildSceneBuffers( commandList );
  RecreateScrenSizeDependantTextures( commandList, width, height );
}

void Scene::TearDownSceneBuffers( CommandList& commandList )
{
  commandList.HoldResource( eastl::move( nodeBuffer ) );
  commandList.HoldResource( eastl::move( rootNodeChildrenInidcesBuffer ) );
  commandList.HoldResource( eastl::move( meshBuffer ) );
  commandList.HoldResource( eastl::move( cameraBuffer ) );
  commandList.HoldResource( eastl::move( indirectOpaqueDrawBuffer ) );
  commandList.HoldResource( eastl::move( indirectOpaqueTwoSidedDrawBuffer ) );
  commandList.HoldResource( eastl::move( indirectOpaqueAlphaTestedDrawBuffer ) );
  commandList.HoldResource( eastl::move( indirectOpaqueTwoSidedAlphaTestedDrawBuffer ) );
  commandList.HoldResource( eastl::move( indirectTranslucentDrawBuffer ) );
  commandList.HoldResource( eastl::move( indirectTranslucentTwoSidedDrawBuffer ) );
}

void Scene::BuildSceneBuffers( CommandList& commandList )
{
  nodeSlots.clear();
  meshSlots.clear();
  cameraSlots.clear();
  rootNodeChildrenIndices.clear();
  rootNodeChildrenIndices.push_back( 0 ); // This will store the number of root node children

  MarshallSceneToGPU( *rootNode, instanceCount, *this );
  
  rootNodeChildrenIndices[ 0 ] = uint32_t( rootNodeChildrenIndices.size() - 1 );

  auto& device = RenderManager::GetInstance().GetDevice();

  nodeBuffer = CreateBufferFromData( nodeSlots.data(), int( nodeSlots.size() ), ResourceType::Buffer, device, commandList, L"nodeBuffer" );
  auto nodeBufferDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView, NodeBufferSlot, *nodeBuffer, sizeof( NodeSlot ) );
  nodeBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( nodeBufferDesc ) );

  rootNodeChildrenInidcesBuffer = CreateBufferFromData( rootNodeChildrenIndices.data(), int( rootNodeChildrenIndices.size() ), ResourceType::Buffer, device, commandList, L"rootNodeChildrenInidcesBuffer" );
  auto rootNodeChildrenInidcesBufferDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView, RootNodeChildrenInidcesBufferSlot, *rootNodeChildrenInidcesBuffer, sizeof( uint32_t ) );
  rootNodeChildrenInidcesBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( rootNodeChildrenInidcesBufferDesc ) );

  meshBuffer = CreateBufferFromData( meshSlots.data(), int( meshSlots.size() ), ResourceType::Buffer, device, commandList, L"meshBuffer" );
  auto meshBufferDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView, MeshBufferSlot, *meshBuffer, sizeof( MeshSlot ) );
  meshBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( meshBufferDesc ) );

  cameraBuffer = CreateBufferFromData( cameraSlots.data(), int( cameraSlots.size() ), ResourceType::Buffer, device, commandList, L"cameraBuffer" );
  auto cameraBufferDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView, CameraBufferSlot, *cameraBuffer, sizeof( CameraSlot ) );
  cameraBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( cameraBufferDesc ) );

  indirectOpaqueDrawBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, sizeof( IndirectRender ) * instanceCount, sizeof( IndirectRender ), L"indirectOpaqueDrawBuffer" );
  auto indirectOpaqueDrawBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, IndirectOpaqueDrawBufferUAVSlot, *indirectOpaqueDrawBuffer, sizeof( IndirectRender ) );
  auto indirectOpaqueDrawBufferSRVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView,  IndirectOpaqueDrawBufferSRVSlot, *indirectOpaqueDrawBuffer, sizeof( IndirectRender ) );
  indirectOpaqueDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( indirectOpaqueDrawBufferUAVDesc ) );
  indirectOpaqueDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView,  eastl::move( indirectOpaqueDrawBufferSRVDesc ) );

  indirectOpaqueTwoSidedDrawBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, sizeof( IndirectRender ) * instanceCount, sizeof( IndirectRender ), L"indirectOpaqueTwoSidedDrawBuffer" );
  auto indirectOpaqueTwoSidedDrawBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, IndirectOpaqueTwoSidedDrawBufferUAVSlot, *indirectOpaqueTwoSidedDrawBuffer, sizeof( IndirectRender ) );
  auto indirectOpaqueTwoSidedDrawBufferSRVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView,  IndirectOpaqueTwoSidedDrawBufferSRVSlot, *indirectOpaqueTwoSidedDrawBuffer, sizeof( IndirectRender ) );
  indirectOpaqueTwoSidedDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( indirectOpaqueTwoSidedDrawBufferUAVDesc ) );
  indirectOpaqueTwoSidedDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView,  eastl::move( indirectOpaqueTwoSidedDrawBufferSRVDesc ) );

  indirectOpaqueAlphaTestedDrawBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, sizeof( IndirectRender ) * instanceCount, sizeof( IndirectRender ), L"indirectOpaqueAlphaTestedDrawBuffer" );
  auto indirectOpaqueAlphaTestedDrawBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, IndirectOpaqueAlphaTestedDrawBufferUAVSlot, *indirectOpaqueAlphaTestedDrawBuffer, sizeof( IndirectRender ) );
  auto indirectOpaqueAlphaTestedDrawBufferSRVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView,  IndirectOpaqueAlphaTestedDrawBufferSRVSlot, *indirectOpaqueAlphaTestedDrawBuffer, sizeof( IndirectRender ) );
  indirectOpaqueAlphaTestedDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( indirectOpaqueAlphaTestedDrawBufferUAVDesc ) );
  indirectOpaqueAlphaTestedDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView,  eastl::move( indirectOpaqueAlphaTestedDrawBufferSRVDesc ) );

  indirectOpaqueTwoSidedAlphaTestedDrawBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, sizeof( IndirectRender ) * instanceCount, sizeof( IndirectRender ), L"indirectOpaqueTwoSidedAlphaTestedDrawBuffer" );
  auto indirectOpaqueTwoSidedAlphaTestedDrawBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, IndirectOpaqueTwoSidedAlphaTestedDrawBufferUAVSlot, *indirectOpaqueTwoSidedAlphaTestedDrawBuffer, sizeof( IndirectRender ) );
  auto indirectOpaqueTwoSidedAlphaTestedDrawBufferSRVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView,  IndirectOpaqueTwoSidedAlphaTestedDrawBufferSRVSlot, *indirectOpaqueTwoSidedAlphaTestedDrawBuffer, sizeof( IndirectRender ) );
  indirectOpaqueTwoSidedAlphaTestedDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( indirectOpaqueTwoSidedAlphaTestedDrawBufferUAVDesc ) );
  indirectOpaqueTwoSidedAlphaTestedDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView,  eastl::move( indirectOpaqueTwoSidedAlphaTestedDrawBufferSRVDesc ) );

  indirectTranslucentDrawBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, sizeof( IndirectRender ) * instanceCount, sizeof( IndirectRender ), L"indirectTranslucentDrawBuffer" );
  auto indirectTranslucentDrawBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, IndirectTranslucentDrawBufferUAVSlot, *indirectTranslucentDrawBuffer, sizeof( IndirectRender ) );
  auto indirectTranslucentDrawBufferSRVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView,  IndirectTranslucentDrawBufferSRVSlot, *indirectTranslucentDrawBuffer, sizeof( IndirectRender ) );
  indirectTranslucentDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( indirectTranslucentDrawBufferUAVDesc ) );
  indirectTranslucentDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView,  eastl::move( indirectTranslucentDrawBufferSRVDesc ) );

  indirectTranslucentTwoSidedDrawBuffer = device.CreateBuffer( ResourceType::Buffer, HeapType::Default, true, sizeof( IndirectRender ) * instanceCount, sizeof( IndirectRender ), L"indirectTranslucentTwoSidedDrawBuffer" );
  auto indirectTranslucentTwoSidedDrawBufferUAVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::UnorderedAccessView, IndirectTranslucentTwoSidedDrawBufferUAVSlot, *indirectTranslucentTwoSidedDrawBuffer, sizeof( IndirectRender ) );
  auto indirectTranslucentTwoSidedDrawBufferSRVDesc = device.GetShaderResourceHeap().RequestDescriptorFromSlot( device, ResourceDescriptorType::ShaderResourceView, IndirectTranslucentTwoSidedDrawBufferSRVSlot, *indirectTranslucentTwoSidedDrawBuffer, sizeof( IndirectRender ) );
  indirectTranslucentTwoSidedDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( indirectTranslucentTwoSidedDrawBufferUAVDesc ) );
  indirectTranslucentTwoSidedDrawBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( indirectTranslucentTwoSidedDrawBufferSRVDesc ) );
}

Upscaling::Quality Scene::GetUpscalingQuality() const
{
  return upscalingQuality;
}

void Scene::SetUpscalingQuality( CommandList& commandList, int width, int height, Upscaling::Quality quality )
{
  if ( upscalingQuality == quality )
    return;

  upscalingQuality = quality;
  RecreateScrenSizeDependantTextures( commandList, width, height );
}

void Scene::TearDownScreenSizeDependantTextures( CommandList& commandList )
{
  auto& manager = RenderManager::GetInstance();
  auto& device = manager.GetDevice();

  commandList.HoldResource( eastl::move( debugTexture ) );
  commandList.HoldResource( eastl::move( motionVectorTexture ) );
  commandList.HoldResource( eastl::move( textureMipTexture ) );
  commandList.HoldResource( eastl::move( geometryIdsTexture ) );
  commandList.HoldResource( eastl::move( lqColorTexture ) );
  commandList.HoldResource( eastl::move( hqColorTexture ) );
  commandList.HoldResource( eastl::move( depthTexture ) );
  commandList.HoldResource( eastl::move( aoTexture ) );
  commandList.HoldResource( eastl::move( reflectionTexture ) );
  commandList.HoldResource( eastl::move( giTexture ) );
  commandList.HoldResource( eastl::move( lumaTexture ) );
  commandList.HoldResource( eastl::move( shadowTexture ) );
  commandList.HoldResource( eastl::move( shadowTransTexture ) );
  for ( auto& t : bloomTextures ) for ( auto& tx : t ) commandList.HoldResource( eastl::move( tx ) );
  if ( denoiser )
    denoiser->TearDown( &commandList );
  denoiser.reset();
}

void Scene::RecreateScrenSizeDependantTextures( CommandList& commandList, int width, int height )
{
  auto& manager = RenderManager::GetInstance();
  auto& device = manager.GetDevice();

  if ( upscaling )
  {
    upscaling->TearDown();
    upscaling.reset();
  }

  if ( upscalingQuality != Upscaling::Quality::Off )
  {
    upscaling = Upscaling::Instantiate();
    upscaling->Initialize( commandList, upscalingQuality, width, height );
  }

  auto lrts = upscaling ? upscaling->GetRenderingResolution() : XMINT2( width, height );

  hqColorTexture = device.Create2DTexture( commandList, width, height, nullptr, 0, RenderManager::HDRFormat, true, UpscaledTextureSlot, UpscaledTextureUAVSlot, false, L"HQColorTexture" );

  if ( upscaling )
    lqColorTexture = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::HDRFormat, true, ColorTextureSlot,  eastl::nullopt, false, L"LQColorTexture");

  motionVectorTexture = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::MotionVectorFormat, true, MotionVectorsSlot, eastl::nullopt, false, L"MotionVectors" );

  textureMipTexture  = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::TextureMipFormat,  true, TextureMipSlot,  eastl::nullopt, false, L"TextureMip" );
  geometryIdsTexture = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::GeometryIdsFormat, true, GeometryIdsSlot, eastl::nullopt, false, L"GeometryIds" );

  depthTexture = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::DepthFormat, false, DepthTextureSlot, eastl::nullopt, false, L"DepthTexture" );
  commandList.ChangeResourceState( *depthTexture, ResourceStateBits::DepthWrite );

  #if USE_AO_WITH_GI
    aoTexture = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::AOFormat,  false, AOTextureSRVSlot, AOTextureUAVSlot, false, L"AOTexture" );
  #endif

  reflectionTexture = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::HDRFormat, false, ReflectionTextureSRVSlot, ReflectionTextureUAVSlot, false, L"ReflectionTexture" );
  giTexture         = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::GIFormat,  false, GITextureSRVSlot, GITextureUAVSlot, false, L"GITexture" );

  auto bloomWidth  = width  > 2560 ? 1280 : 640;
  auto bloomHeight = height > 1440 ? 768  : 384;

  for ( int bt = 0; bt < 5; ++bt )
  {
    bloomTextures[ bt ][ 0 ] = device.Create2DTexture( commandList, bloomWidth >> bt, bloomHeight >> bt, nullptr, 0, RenderManager::HDRFormat, true, BloomA0TextureSlot + bt, BloomA0TextureUAVSlot + bt, false, L"Bloom_a" );
    bloomTextures[ bt ][ 1 ] = device.Create2DTexture( commandList, bloomWidth >> bt, bloomHeight >> bt, nullptr, 0, RenderManager::HDRFormat, true, BloomB0TextureSlot + bt, BloomB0TextureUAVSlot + bt, false, L"Bloom_b" );
  }

  lumaTexture = device.Create2DTexture( commandList, bloomWidth, bloomHeight, nullptr, 0, RenderManager::LumaFormat, true, LumaTextureSlot, LumaTextureUAVSlot, false, L"Luma" );

  shadowTexture = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::ShadowFormat, true, ShadowTextureSlot, ShadowTextureUAVSlot, false, L"Shadow" );
  shadowTransTexture = device.Create2DTexture( commandList, lrts.x, lrts.y, nullptr, 0, RenderManager::ShadowTransFormat, true, ShadowTransTextureSlot, ShadowTransTextureUAVSlot, false, L"ShadowTrans" );

  denoiser = CreateDenoiser( device, manager.GetCommandQueue( CommandQueueType::Direct ), commandList, lrts.x, lrts.y );
}

void Scene::CreateBRDFLUTTexture( CommandList& commandList )
{
  if ( specBRDFLUTTexture )
    return;

  specBRDFLUTTexture = RenderManager::GetInstance().GetDevice().Create2DTexture( commandList, SpecBRDFLUTSize, SpecBRDFLUTSize, nullptr, 0, PixelFormat::RG1616F, false, SpecBRDFLUTSlot, SpecBRDFLUTUAVSlot, false, L"SpecBRDFLUT" );

  commandList.ChangeResourceState( *specBRDFLUTTexture, ResourceStateBits::UnorderedAccess );
  commandList.SetComputeShader( *specBRDFLUTShader );
  commandList.SetComputeUnorderedAccessView( 0, *specBRDFLUTTexture );
  commandList.Dispatch( SpecBRDFLUTSize / 32, SpecBRDFLUTSize / 32, 1 );
  commandList.AddUAVBarrier( { *specBRDFLUTTexture } );
  commandList.ChangeResourceState( *specBRDFLUTTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );
}

void Scene::CullScene( CommandList& commandList, float jitterX, float jitterY, int targetWidth, int targetHeight, bool useTextureFeedback, bool freezeCulling )
{
  GPUSection gpuSection( commandList, L"Scene culling" );

  commandList.ChangeResourceState( { { *nodeBuffer,                                  ResourceStateBits::NonPixelShaderInput }
                                   , { *rootNodeChildrenInidcesBuffer,               ResourceStateBits::NonPixelShaderInput }
                                   , { *meshBuffer,                                  ResourceStateBits::NonPixelShaderInput }
                                   , { *cameraBuffer,                                ResourceStateBits::NonPixelShaderInput }
                                   , { *lightBuffer,                                 ResourceStateBits::NonPixelShaderInput }
                                   , { *materialBuffer,                              ResourceStateBits::NonPixelShaderInput }
                                   , { *indirectOpaqueDrawBuffer,                    ResourceStateBits::UnorderedAccess }
                                   , { *indirectOpaqueTwoSidedDrawBuffer,            ResourceStateBits::UnorderedAccess }
                                   , { *indirectOpaqueAlphaTestedDrawBuffer,         ResourceStateBits::UnorderedAccess }
                                   , { *indirectOpaqueTwoSidedAlphaTestedDrawBuffer, ResourceStateBits::UnorderedAccess }
                                   , { *indirectTranslucentDrawBuffer,               ResourceStateBits::UnorderedAccess }
                                   , { *indirectTranslucentTwoSidedDrawBuffer,       ResourceStateBits::UnorderedAccess }
                                   , { *indirectDrawCountBuffer,                     ResourceStateBits::UnorderedAccess }
                                   , { *frameParamsBuffer,                           ResourceStateBits::UnorderedAccess }
                                   , { *lightParamsBuffer,                           ResourceStateBits::UnorderedAccess }
                                   , { *skyBuffer,                                   ResourceStateBits::UnorderedAccess } } );

  struct
  {
    XMFLOAT4X4 cameraViewProj;
    XMFLOAT4X4 cameraInvViewProj;

    float    jitterX;
    float    jitterY;
    uint32_t viewportWidth;
    uint32_t viewportHeight;
    uint32_t feedbackPhase;
    uint32_t frameIndex;
    uint32_t freeze;
    uint32_t cameraIndex;
  } prepareCullingParams;

  static uint32_t feedbackPhase = 0;

  #if ENABLE_TEXTURE_STREAMING
    if ( ++feedbackPhase > 10 )
      feedbackPhase = 1;
  #endif

  jitterX *=  2.0f / targetWidth;
  jitterY *= -2.0f / targetHeight;

  auto cameraNode    = FindNodeByName( "Camera" );
  auto nodeTransform = cameraNode->GetTransform();

  auto cameraView = XMMatrixInverse( nullptr, nodeTransform );
  XMMATRIX cameraProj;
  cameraNode->ForEachCamera( [ &cameraProj ]( const Camera& camera )
  {
    cameraProj = camera.GetProjTransform();
    return false;
  } );

  cameraProj.r[ 2 ] = XMVectorAdd( cameraProj.r[ 2 ], XMVectorSet( jitterX, jitterY, 0, 0 ) );

  auto cameraViewProj    = XMMatrixMultiply( cameraView, cameraProj );
  auto cameraInvViewProj = XMMatrixInverse( nullptr, cameraViewProj );

  XMStoreFloat4x4( &prepareCullingParams.cameraViewProj, cameraViewProj );
  XMStoreFloat4x4( &prepareCullingParams.cameraInvViewProj, cameraInvViewProj );

  prepareCullingParams.jitterX        = jitterX;
  prepareCullingParams.jitterY        = jitterY;
  prepareCullingParams.viewportWidth  = targetWidth;
  prepareCullingParams.viewportHeight = targetHeight;
  prepareCullingParams.feedbackPhase  = useTextureFeedback ? feedbackPhase : 0xFFFFFFFFU;
  prepareCullingParams.frameIndex     = frameCounter;
  prepareCullingParams.freeze         = freezeCulling ? 1 : 0;
  prepareCullingParams.cameraIndex    = cameraNodeIndex;

  commandList.SetComputeShader( *prepareCullingShader );
  commandList.SetComputeConstantValues( 0, prepareCullingParams, 0 );
  commandList.SetComputeUnorderedAccessView( 1, *indirectDrawCountBuffer );
  commandList.SetComputeUnorderedAccessView( 2, *frameParamsBuffer );
  commandList.SetComputeShaderResourceView( 3, *nodeBuffer );
  commandList.SetComputeShaderResourceView( 4, *cameraBuffer );
  commandList.Dispatch( 1, 1, 1 );

  commandList.AddUAVBarrier( { *indirectDrawCountBuffer, *frameParamsBuffer } );

  if ( !prepareCullingParams.freeze )
  {
    // Do clipping on instances, the result goes to the indirect args buffer
    commandList.SetComputeShader( *cullingShader );
    commandList.SetComputeShaderResourceView( 0, *nodeBuffer );
    commandList.SetComputeShaderResourceView( 1, *meshBuffer );
    commandList.SetComputeShaderResourceView( 2, *lightBuffer );
    commandList.SetComputeShaderResourceView( 3, *materialBuffer );
    commandList.SetComputeShaderResourceView( 4, *rootNodeChildrenInidcesBuffer );
    commandList.SetComputeUnorderedAccessView( 5, *indirectOpaqueDrawBuffer );
    commandList.SetComputeUnorderedAccessView( 6, *indirectOpaqueTwoSidedDrawBuffer );
    commandList.SetComputeUnorderedAccessView( 7, *indirectOpaqueAlphaTestedDrawBuffer );
    commandList.SetComputeUnorderedAccessView( 8, *indirectOpaqueTwoSidedAlphaTestedDrawBuffer );
    commandList.SetComputeUnorderedAccessView( 9, *indirectTranslucentDrawBuffer );
    commandList.SetComputeUnorderedAccessView( 10, *indirectTranslucentTwoSidedDrawBuffer );
    commandList.SetComputeUnorderedAccessView( 11, *indirectDrawCountBuffer );
    commandList.SetComputeUnorderedAccessView( 12, *frameParamsBuffer );
    commandList.SetComputeUnorderedAccessView( 13, *lightParamsBuffer );
    commandList.SetComputeUnorderedAccessView( 14, *skyBuffer );

    commandList.Dispatch( TG( rootNodeChildrenIndices.size(), CullingKernelWidth ), 1, 1 );
  }

  commandList.AddUAVBarrier( { *indirectOpaqueDrawBuffer
                             , *indirectOpaqueTwoSidedDrawBuffer
                             , *indirectOpaqueAlphaTestedDrawBuffer
                             , *indirectOpaqueTwoSidedAlphaTestedDrawBuffer
                             , *indirectTranslucentDrawBuffer
                             ,* indirectTranslucentTwoSidedDrawBuffer
                             , *indirectDrawCountBuffer
                             , *frameParamsBuffer
                             , *lightParamsBuffer
                             , *skyBuffer } );
}

void Scene::RenderSkyToCube( CommandList& commandList )
{
  GPUSection gpuSection( commandList, L"Render sky to cube" );

  commandList.ChangeResourceState( { { *skyBuffer,         ResourceStateBits::VertexOrConstantBuffer }
                                   , { *frameParamsBuffer, ResourceStateBits::VertexOrConstantBuffer }
                                   , { *skyTexture,        ResourceStateBits::RenderTarget } } );

  commandList.SetPipelineState( RenderManager::GetInstance().GetPipelinePreset( PipelinePresets::SkyCube ) );
  commandList.SetPrimitiveType( PrimitiveType::TriangleList );
  commandList.SetVertexBufferToNull();
  commandList.SetIndexBufferToNull();
  commandList.SetConstantBuffer( 0, *skyBuffer );
  commandList.SetConstantBuffer( 1, *frameParamsBuffer );

  commandList.SetViewport( 0, 0, skyTexture->GetTextureWidth(), skyTexture->GetTextureWidth() );
  commandList.SetScissor ( 0, 0, skyTexture->GetTextureWidth(), skyTexture->GetTextureWidth() );

  for ( int face = 0; face < 6; ++face )
  {
    struct
    {
      XMFLOAT4X4 vp;
      uint32_t   isCube = true;
    } skyConstants;

    auto view = XMMatrixLookToLH( XMVectorZero(), XMLoadFloat3( &cubeLookAt[ face ] ), XMLoadFloat3( &cubeUpDir[ face ] ) );
    auto proj = XMMatrixPerspectiveFovLH( XM_PIDIV2, 1, 1, 1000 );
    XMStoreFloat4x4( &skyConstants.vp, view * proj );

    unsigned cubeSide = face + 1;
    auto namedSlot = ResourceDescriptorType( int( ResourceDescriptorType::RenderTargetView0 ) + face );
    commandList.SetRenderTarget( *skyTexture->GetResourceDescriptor( namedSlot ), nullptr );
    commandList.SetConstantValues( 2, skyConstants, 0 );
    commandList.Draw( 36 );
  }
}

void Scene::ClearTexturesAndPrepareRendering( CommandList& commandList, Resource& renderTarget )
{
  GPUSection gpuSection( commandList, L"Clear depth and prepare rendering" );

  commandList.ChangeResourceState( { {  renderTarget, ResourceStateBits::RenderTarget }
                                   , { *depthTexture, ResourceStateBits::DepthWrite }
                                   , { *motionVectorTexture,  ResourceStateBits::RenderTarget }
                                   , { *textureMipTexture,    ResourceStateBits::RenderTarget } 
                                   , { *geometryIdsTexture,   ResourceStateBits::RenderTarget } } );

  #if USE_REVERSE_PROJECTION
    commandList.ClearDepthStencil( *depthTexture, 0 );
  #else
    commandList.ClearDepthStencil( *depthTexture, 1 );
  #endif

  commandList.ClearRenderTarget( renderTarget, Color() );
  commandList.ClearRenderTarget( *motionVectorTexture, Color() );
  commandList.ClearRenderTarget( *textureMipTexture, Color( 255, 0, 0, 0 ) );
  commandList.ClearRenderTarget( *geometryIdsTexture, Color() );
  commandList.ClearRenderTarget( *motionVectorTexture, Color() );

  commandList.ChangeResourceState( { { *indirectOpaqueDrawBuffer,                    ResourceStateBits::IndirectArgument }
                                   , { *indirectOpaqueTwoSidedDrawBuffer,            ResourceStateBits::IndirectArgument }
                                   , { *indirectOpaqueAlphaTestedDrawBuffer,         ResourceStateBits::IndirectArgument }
                                   , { *indirectOpaqueTwoSidedAlphaTestedDrawBuffer, ResourceStateBits::IndirectArgument }
                                   , { *indirectTranslucentDrawBuffer,               ResourceStateBits::IndirectArgument }
                                   , { *indirectTranslucentTwoSidedDrawBuffer,       ResourceStateBits::IndirectArgument }
                                   , { *indirectDrawCountBuffer,                     ResourceStateBits::IndirectArgument }
                                   , { *lightParamsBuffer,                           ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *materialBuffer,                              ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *skyTexture,                                  ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput } } );
}

void Scene::RenderDepth( CommandList& commandList )
{
  auto& renderManager = RenderManager::GetInstance();

  GPUSection gpuSection( commandList, L"Render depth prepass" );

  commandList.SetRenderTarget( { motionVectorTexture.get(), textureMipTexture.get(), geometryIdsTexture.get() }, depthTexture.get() );
  
  commandList.SetPrimitiveType( PrimitiveType::TriangleList );
  commandList.SetVertexBufferToNull();
  commandList.SetIndexBufferToNull();

  auto drawPass = [&]( PipelinePresets pipeline, CommandSignatures sig, Resource& drawBuffer, int offset )
  {
    commandList.SetPipelineState( renderManager.GetPipelinePreset( pipeline ) );
    commandList.SetConstantBuffer( 1, *frameParamsBuffer );
    commandList.SetShaderResourceView( 2, *materialBuffer );
    commandList.SetDescriptorHeap( 3, renderManager.GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
    commandList.SetDescriptorHeap( 4, renderManager.GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
    commandList.SetDescriptorHeap( 5, renderManager.GetShaderResourceHeap(), Scene2DResourceBaseSlot );
    commandList.SetDescriptorHeap( 6, renderManager.GetShaderResourceHeap(), Scene2DMipTailBaseSlot );
    commandList.SetDescriptorHeap( 7, renderManager.GetShaderResourceHeap(), Engine2DTileTexturesBaseSlot );
    commandList.SetDescriptorHeap( 8, renderManager.GetShaderResourceHeap(), Engine2DReferenceTextureBaseSlot );
    commandList.ExecuteIndirect( renderManager.GetCommandSignature( sig ), drawBuffer, 0, *indirectDrawCountBuffer, sizeof( uint32_t ) * offset, instanceCount );
  };

  drawPass( PipelinePresets::MeshDepth,         CommandSignatures::MeshDepth,         *indirectOpaqueDrawBuffer,         0 );
  drawPass( PipelinePresets::MeshDepthTwoSided, CommandSignatures::MeshDepthTwoSided, *indirectOpaqueTwoSidedDrawBuffer, 1 );
  drawPass( PipelinePresets::MeshDepthAlphaTest, CommandSignatures::MeshDepthAlphaTest, *indirectOpaqueAlphaTestedDrawBuffer, 2 );
  drawPass( PipelinePresets::MeshDepthTwoSidedAlphaTest, CommandSignatures::MeshDepthTwoSidedAlphaTest, *indirectOpaqueTwoSidedAlphaTestedDrawBuffer, 3 );
}

void Scene::RenderShadow( CommandList& commandList )
{
  auto& renderManager = RenderManager::GetInstance();

  GPUSection gpuSection( commandList, L"Render shadow" );

  commandList.ChangeResourceState( { { *shadowTexture,      ResourceStateBits::UnorderedAccess }
                                   , { *shadowTransTexture, ResourceStateBits::UnorderedAccess }
                                   , { *depthTexture,       ResourceStateBits::NonPixelShaderInput }
                                   , { *frameParamsBuffer,  ResourceStateBits::VertexOrConstantBuffer }
                                   , { *materialBuffer,     ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *lightParamsBuffer,  ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput } } );

  commandList.SetRayTracingShader( *traceShadowShader );
  commandList.SetComputeConstantBuffer( 0, *frameParamsBuffer );
  commandList.SetComputeRayTracingScene( 1, *tlas );
  commandList.SetComputeShaderResourceView( 2, *modelMetaBuffer );
  commandList.SetComputeShaderResourceView( 3, *materialBuffer );
  commandList.SetComputeShaderResourceView( 4, *lightParamsBuffer );
  commandList.SetComputeShaderResourceView( 5, *depthTexture );
  commandList.SetComputeShaderResourceView( 6, *scramblingRankingTexture );
  commandList.SetComputeShaderResourceView( 7, *sobolTexture );
  commandList.SetComputeUnorderedAccessView( 8, *shadowTexture );
  commandList.SetComputeUnorderedAccessView( 9, *shadowTransTexture );
  commandList.SetComputeDescriptorHeap( 10, RenderManager::GetInstance().GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
  commandList.SetComputeDescriptorHeap( 11, RenderManager::GetInstance().GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
  commandList.SetComputeDescriptorHeap( 12, RenderManager::GetInstance().GetShaderResourceHeap(), Scene2DResourceBaseSlot );
  commandList.SetComputeDescriptorHeap( 13, RenderManager::GetInstance().GetShaderResourceHeap(), Scene2DMipTailBaseSlot );
  commandList.SetComputeDescriptorHeap( 14, RenderManager::GetInstance().GetShaderResourceHeap(), Engine2DTileTexturesBaseSlot );

  commandList.DispatchRays( shadowTexture->GetTextureWidth(), shadowTexture->GetTextureHeight(), 1 );

  commandList.AddUAVBarrier( { *shadowTexture, *shadowTransTexture } );

  commandList.ChangeResourceState( { { *shadowTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *shadowTransTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { * depthTexture, ResourceStateBits::DepthRead } } );
}

void Scene::RenderAO( CommandList& commandList )
{
  #if USE_AO_WITH_GI
    auto& renderManager = RenderManager::GetInstance();

    GPUSection gpuSection( commandList, L"Render ambient occlusion" );

    commandList.ChangeResourceState( *aoTexture, ResourceStateBits::UnorderedAccess );

    commandList.SetRayTracingShader( *traceAOShader );
    commandList.SetComputeUnorderedAccessView( triangleExtractRootConstants + 0, *aoTexture );
    commandList.SetComputeConstantValues( triangleExtractRootConstants + 1, denoiser->GetHitDistanceParams(), 0 );

    SetupTriangleBuffers( commandList, true );

    commandList.DispatchRays( aoTexture->GetTextureWidth() / 2, aoTexture->GetTextureHeight(), 1 );

    commandList.AddUAVBarrier( { *aoTexture } );
    commandList.ChangeResourceState( *aoTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );
  #endif
}

void Scene::RenderGI( CommandList& commandList )
{
  auto& renderManager = RenderManager::GetInstance();

  GPUSection gpuSection( commandList, L"Render global illumination" );

  commandList.ChangeResourceState( *giTexture, ResourceStateBits::UnorderedAccess );

  commandList.SetRayTracingShader( *traceGIShader );
  commandList.SetComputeShaderResourceView( triangleExtractRootConstants + 0, *lightParamsBuffer );
  commandList.SetComputeShaderResourceView( triangleExtractRootConstants + 1, *specBRDFLUTTexture );
  commandList.SetComputeShaderResourceView( triangleExtractRootConstants + 2, *skyTexture );
  commandList.SetComputeUnorderedAccessView( triangleExtractRootConstants + 3, *giTexture );
  commandList.SetComputeConstantValues( triangleExtractRootConstants + 4, denoiser->GetHitDistanceParams(), 0 );

  SetupTriangleBuffers( commandList, true );

  commandList.DispatchRays( giTexture->GetTextureWidth() / 2, giTexture->GetTextureHeight(), 1 );

  commandList.AddUAVBarrier( { *giTexture } );

  commandList.ChangeResourceState( *giTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );
}

void Scene::Denoise( CommandAllocator& commandAllocator, CommandList& commandList, float jitterX, float jitterY, bool showDenoiserDebugLayer )
{
  auto cameraNode    = FindNodeByName( "Camera" );
  auto nodeTransform = cameraNode->GetTransform();

  auto cameraView = XMMatrixInverse( nullptr, nodeTransform );
  XMMATRIX cameraProj;
  float nearZ, farZ;
  cameraNode->ForEachCamera( [&]( const Camera& camera )
  {
    cameraProj = camera.GetProjTransform();
    nearZ      = camera.GetNearZ();
    farZ       = camera.GetFarZ();
    return false;
  } );

  denoiser->Preprocess( commandList
                      , [this]( ComputeShader& computeShader, CommandList& commandList )
                        {
                          commandList.SetComputeShader( computeShader );
                          SetupTriangleBuffers( commandList, true ); return triangleExtractRootConstants;
                        }
                      , nearZ
                      , farZ );
  auto denoisedTextures = denoiser->Denoise( commandAllocator
                                            , commandList
                                            , *giTexture
                                            , aoTexture.get()
                                            , *shadowTexture
                                            , *shadowTransTexture
                                            , *reflectionTexture
                                            , cameraView
                                            , cameraProj
                                            , jitterX
                                            , jitterY
                                            , frameCounter
, showDenoiserDebugLayer );

  denoisedAOTexture         = denoisedTextures.ambientOcclusion;
  denoisedShadowTexture     = denoisedTextures.shadow;
  denoisedReflectionTexture = denoisedTextures.reflection;
  denoisedGITexture         = denoisedTextures.globalIllumination;
  denoiserValidationTexture = denoisedTextures.validation;

  commandList.BindHeaps();
}

void Scene::RenderReflection( CommandList& commandList )
{
  auto& renderManager = RenderManager::GetInstance();

  GPUSection gpuSection( commandList, L"Render reflection" );

  commandList.ChangeResourceState( *reflectionTexture, ResourceStateBits::UnorderedAccess );

  commandList.SetRayTracingShader( *traceReflectionShader );
  commandList.SetComputeShaderResourceView( triangleExtractRootConstants + 0, *lightParamsBuffer );
  commandList.SetComputeShaderResourceView( triangleExtractRootConstants + 1, *specBRDFLUTTexture );
  commandList.SetComputeShaderResourceView( triangleExtractRootConstants + 2, *skyTexture );
  commandList.SetComputeUnorderedAccessView( triangleExtractRootConstants + 3, *reflectionTexture );
  commandList.SetComputeConstantValues( triangleExtractRootConstants + 4, denoiser->GetHitDistanceParams(), 0 );

  SetupTriangleBuffers( commandList, true );

  commandList.DispatchRays( reflectionTexture->GetTextureWidth() / 2, reflectionTexture->GetTextureHeight(), 1 );

  commandList.AddUAVBarrier( { *reflectionTexture } );

  commandList.ChangeResourceState( *reflectionTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );
}

void Scene::SetupTriangleBuffers( CommandList& commandList, bool compute )
{
  auto& manager = RenderManager::GetInstance();

  commandList.ChangeResourceState( { { *depthTexture,                                ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *textureMipTexture,                           ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput } 
                                   , { *geometryIdsTexture,                          ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *indirectOpaqueDrawBuffer,                    ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *indirectOpaqueTwoSidedDrawBuffer,            ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *indirectOpaqueAlphaTestedDrawBuffer,         ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *indirectOpaqueTwoSidedAlphaTestedDrawBuffer, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput } } );

  if ( compute )
  {
    commandList.SetComputeConstantBuffer( 0, *frameParamsBuffer );
    commandList.SetComputeRayTracingScene( 1, *tlas );
    commandList.SetComputeShaderResourceView( 2, *materialBuffer );
    commandList.SetComputeShaderResourceView( 3, *modelMetaBuffer );
    commandList.SetComputeShaderResourceView( 4, *depthTexture );
    commandList.SetComputeShaderResourceView( 5, *textureMipTexture );
    commandList.SetComputeShaderResourceView( 6, *geometryIdsTexture );
    commandList.SetComputeDescriptorHeap( 7,  manager.GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
    commandList.SetComputeDescriptorHeap( 8,  manager.GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
    commandList.SetComputeDescriptorHeap( 9,  manager.GetShaderResourceHeap(), Scene2DResourceBaseSlot );
    commandList.SetComputeDescriptorHeap( 10, manager.GetShaderResourceHeap(), Scene2DMipTailBaseSlot );
    commandList.SetComputeDescriptorHeap( 11, manager.GetShaderResourceHeap(), Engine2DTileTexturesBaseSlot );
    commandList.SetComputeDescriptorHeap( 12, manager.GetShaderResourceHeap(), IndirectOpaqueDrawBufferSRVSlot );
  }
  else
  {
    commandList.SetConstantBuffer( 0, *frameParamsBuffer );
    commandList.SetRayTracingScene( 1, *tlas );
    commandList.SetShaderResourceView( 2, *materialBuffer );
    commandList.SetShaderResourceView( 3, *modelMetaBuffer );
    commandList.SetShaderResourceView( 4, *depthTexture );
    commandList.SetShaderResourceView( 5, *textureMipTexture );
    commandList.SetShaderResourceView( 6, *geometryIdsTexture );
    commandList.SetDescriptorHeap( 7,  manager.GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
    commandList.SetDescriptorHeap( 8,  manager.GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
    commandList.SetDescriptorHeap( 9,  manager.GetShaderResourceHeap(), Scene2DResourceBaseSlot );
    commandList.SetDescriptorHeap( 10, manager.GetShaderResourceHeap(), Scene2DMipTailBaseSlot );
    commandList.SetDescriptorHeap( 11, manager.GetShaderResourceHeap(), Engine2DTileTexturesBaseSlot );
    commandList.SetDescriptorHeap( 12, manager.GetShaderResourceHeap(), IndirectOpaqueDrawBufferSRVSlot );
  }
}

void Scene::RenderDirectLighting( CommandList& commandList )
{
  auto& renderManager = RenderManager::GetInstance();

  GPUSection gpuSection( commandList, L"Direct lighting" );

  if ( denoisedAOTexture )
    commandList.ChangeResourceState( *denoisedAOTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );

  if ( denoisedShadowTexture )
    commandList.ChangeResourceState( *denoisedShadowTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );

  if ( denoisedReflectionTexture )
    commandList.ChangeResourceState( *denoisedReflectionTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );

  if ( denoisedGITexture )
    commandList.ChangeResourceState( *denoisedGITexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );

  commandList.SetPipelineState( renderManager.GetPipelinePreset( PipelinePresets::DirectLighting ) );
  commandList.SetPrimitiveType( PrimitiveType::TriangleList );
  commandList.SetVertexBufferToNull();
  commandList.SetIndexBufferToNull();

  commandList.SetShaderResourceView( triangleExtractRootConstants + 0, *lightParamsBuffer );
  commandList.SetShaderResourceView( triangleExtractRootConstants + 1, *specBRDFLUTTexture );
  commandList.SetShaderResourceView( triangleExtractRootConstants + 2, *skyTexture );
  if ( denoisedAOTexture )
    commandList.SetShaderResourceView( triangleExtractRootConstants + 3, *denoisedAOTexture );
  if ( denoisedShadowTexture )
    commandList.SetShaderResourceView( triangleExtractRootConstants + 4, *denoisedShadowTexture );
  if ( denoisedReflectionTexture )
    commandList.SetShaderResourceView( triangleExtractRootConstants + 5, *denoisedReflectionTexture );
  if ( denoisedGITexture )
    commandList.SetShaderResourceView( triangleExtractRootConstants + 6, *denoisedGITexture );
  if ( auto globalTextureFeedbackBuffer = renderManager.GetGlobalTextureFeedbackBuffer( commandList ) )
    commandList.SetUnorderedAccessView( triangleExtractRootConstants + 7, *globalTextureFeedbackBuffer );
  commandList.SetDescriptorHeap( triangleExtractRootConstants + 8, RenderManager::GetInstance().GetShaderResourceHeap(), Scene2DFeedbackBaseSlot );

  SetupTriangleBuffers( commandList, false );
  
  commandList.Draw( 3 );
}

void Scene::RenderTranslucent( CommandList& commandList )
{
  auto& renderManager = RenderManager::GetInstance();

  GPUSection gpuSection( commandList, L"Render translucent models" );

  for ( auto twoSided : { false, true } )
  {
    commandList.SetPrimitiveType( PrimitiveType::TriangleList );
    commandList.SetVertexBufferToNull();
    commandList.SetIndexBufferToNull();
    commandList.SetPipelineState( renderManager.GetPipelinePreset( twoSided ? PipelinePresets::MeshTranslucentTwoSided : PipelinePresets::MeshTranslucent ) );
    commandList.SetConstantBuffer( 1, *frameParamsBuffer );
    commandList.SetRayTracingScene( 2, *tlas );
    commandList.SetShaderResourceView( 3, *materialBuffer );
    commandList.SetShaderResourceView( 4, *lightParamsBuffer );
    commandList.SetShaderResourceView( 5, *modelMetaBuffer );
    commandList.SetShaderResourceView( 6, *specBRDFLUTTexture );
    commandList.SetShaderResourceView( 7, *skyTexture );
    commandList.SetDescriptorHeap( 8,  renderManager.GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
    commandList.SetDescriptorHeap( 9,  renderManager.GetShaderResourceHeap(), SceneBufferResourceBaseSlot );
    commandList.SetDescriptorHeap( 10, renderManager.GetShaderResourceHeap(), Scene2DResourceBaseSlot );
    commandList.SetDescriptorHeap( 11, renderManager.GetShaderResourceHeap(), Scene2DMipTailBaseSlot );
    commandList.SetDescriptorHeap( 12, renderManager.GetShaderResourceHeap(), Engine2DTileTexturesBaseSlot );
    commandList.SetDescriptorHeap( 13, renderManager.GetShaderResourceHeap(), Engine2DReferenceTextureBaseSlot );
    if ( auto globalTextureFeedbackBuffer = renderManager.GetGlobalTextureFeedbackBuffer( commandList ) )
      commandList.SetUnorderedAccessView( 14, *globalTextureFeedbackBuffer );
    commandList.SetDescriptorHeap( 15, RenderManager::GetInstance().GetShaderResourceHeap(), Scene2DFeedbackBaseSlot );
    commandList.ExecuteIndirect( renderManager.GetCommandSignature( twoSided ? CommandSignatures::MeshTranslucentTwoSided : CommandSignatures::MeshTranslucent )
                               , twoSided ? *indirectTranslucentTwoSidedDrawBuffer : *indirectTranslucentDrawBuffer
                               , 0
                               , *indirectDrawCountBuffer
                               , sizeof( uint32_t ) * ( twoSided ? 5 : 4 )
                               , instanceCount );
  }
}

void Scene::RenderSky( CommandList& commandList )
{
  GPUSection gpuSection( commandList, L"Render sky" );

  commandList.ChangeResourceState( *depthTexture, ResourceStateBits::DepthRead );

  struct
  {
    XMFLOAT4X4 vp;
    uint32_t   isCube = false;
  } skyConstants;

  unsigned cubeSide = 0;
  commandList.SetPrimitiveType( PrimitiveType::TriangleList );
  commandList.SetVertexBufferToNull();
  commandList.SetIndexBufferToNull();
  commandList.SetPipelineState( RenderManager::GetInstance().GetPipelinePreset( PipelinePresets::Sky ) );
  commandList.SetConstantBuffer( 0, *skyBuffer );
  commandList.SetConstantBuffer( 1, *frameParamsBuffer );
  commandList.SetConstantValues( 2, skyConstants, 0 );
  commandList.Draw( 36 );
}

void Scene::PostProcessing( CommandList& commandList, Resource& backBuffer )
{
  {
    GPUSection gpuSection( commandList, L"Extract bloom" );

    commandList.ChangeResourceState( { { *hqColorTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                     , { *exposureBuffer, ResourceStateBits::VertexOrConstantBuffer }
                                     , { *bloomTextures[ 0 ][ 0 ], ResourceStateBits::UnorderedAccess }
                                     , { *lumaTexture, ResourceStateBits::UnorderedAccess } } );

    struct
    {
      float invOutputWidth;
      float invOutputHeight;
      float bloomThreshold;
    } extractParams;

    extractParams.invOutputWidth  = 1.0f / bloomTextures[ 0 ][ 0 ]->GetTextureWidth();
    extractParams.invOutputHeight = 1.0f / bloomTextures[ 0 ][ 0 ]->GetTextureHeight();
    extractParams.bloomThreshold  = bloomThreshold;

    commandList.SetComputeShader( *extractBloomShader );
    commandList.SetComputeConstantValues( 0, extractParams, 0 );
    commandList.SetComputeConstantBuffer( 1, *exposureBuffer );
    commandList.SetComputeShaderResourceView( 2, *hqColorTexture );
    commandList.SetComputeUnorderedAccessView( 3, *bloomTextures[ 0 ][ 0 ] );
    commandList.SetComputeUnorderedAccessView( 4, *lumaTexture );
    commandList.Dispatch( ( bloomTextures[ 0 ][ 0 ]->GetTextureWidth () + ExtractBloomKernelWidth - 1  ) / ExtractBloomKernelWidth
                        , ( bloomTextures[ 0 ][ 0 ]->GetTextureHeight() + ExtractBloomKernelHeight - 1 ) / ExtractBloomKernelHeight
                        , 1 );

    commandList.AddUAVBarrier( { *bloomTextures[ 0 ][ 0 ], *lumaTexture } );
  }

  {
    GPUSection gpuSection( commandList, L"Downsample bloom" );

    commandList.ChangeResourceState( { { *bloomTextures[ 0 ][ 0 ], ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                     , { *bloomTextures[ 1 ][ 0 ], ResourceStateBits::UnorderedAccess }
                                     , { *bloomTextures[ 2 ][ 0 ], ResourceStateBits::UnorderedAccess }
                                     , { *bloomTextures[ 3 ][ 0 ], ResourceStateBits::UnorderedAccess }
                                     , { *bloomTextures[ 4 ][ 0 ], ResourceStateBits::UnorderedAccess } } );

    struct
    {
      float invOutputWidth;
      float invOutputHeight;
    } downsampleParams;

    downsampleParams.invOutputWidth  = 1.0f / bloomTextures[ 0 ][ 0 ]->GetTextureWidth();
    downsampleParams.invOutputHeight = 1.0f / bloomTextures[ 0 ][ 0 ]->GetTextureHeight();

    commandList.SetComputeShader( *downsampleBloomShader );
    commandList.SetComputeConstantValues( 0, downsampleParams, 0 );
    commandList.SetComputeShaderResourceView( 1, *bloomTextures[ 0 ][ 0 ] );
    commandList.SetComputeUnorderedAccessView( 2, *bloomTextures[ 1 ][ 0 ] );
    commandList.SetComputeUnorderedAccessView( 3, *bloomTextures[ 2 ][ 0 ] );
    commandList.SetComputeUnorderedAccessView( 4, *bloomTextures[ 3 ][ 0 ] );
    commandList.SetComputeUnorderedAccessView( 5, *bloomTextures[ 4 ][ 0 ] );
    commandList.Dispatch( ( bloomTextures[ 0 ][ 0 ]->GetTextureWidth () / 2 + ExtractBloomKernelWidth - 1  ) / ExtractBloomKernelWidth
                        , ( bloomTextures[ 0 ][ 0 ]->GetTextureHeight() / 2 + ExtractBloomKernelHeight - 1 ) / ExtractBloomKernelHeight
                        , 1 );

    commandList.AddUAVBarrier( { *bloomTextures[ 1 ][ 0 ], *bloomTextures[ 2 ][ 0 ], *bloomTextures[ 3 ][ 0 ], *bloomTextures[ 4 ][ 0 ] } );
  }

  {
    GPUSection gpuSection( commandList, L"Blur bloom" );

    commandList.ChangeResourceState( *bloomTextures[ 4 ][ 0 ], ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );
    commandList.ChangeResourceState( *bloomTextures[ 4 ][ 1 ], ResourceStateBits::UnorderedAccess );

    struct
    {
      float invOutputWidth;
      float invOutputHeight;
    } blurParams;

    blurParams.invOutputWidth  = 1.0f / bloomTextures[ 4 ][ 0 ]->GetTextureWidth();
    blurParams.invOutputHeight = 1.0f / bloomTextures[ 4 ][ 0 ]->GetTextureHeight();

    commandList.SetComputeShader( *blurBloomShader );
    commandList.SetComputeConstantValues( 0, blurParams, 0 );
    commandList.SetComputeShaderResourceView( 1, *bloomTextures[ 4 ][ 0 ] );
    commandList.SetComputeUnorderedAccessView( 2, *bloomTextures[ 4 ][ 1 ] );
    commandList.Dispatch( ( bloomTextures[ 4 ][ 0 ]->GetTextureWidth () + ExtractBloomKernelWidth - 1  ) / ExtractBloomKernelWidth
                        , ( bloomTextures[ 4 ][ 0 ]->GetTextureHeight() + ExtractBloomKernelHeight - 1 ) / ExtractBloomKernelHeight
                        , 1 );

    commandList.AddUAVBarrier( { *bloomTextures[ 4 ][ 1 ] } );
  }

  for ( int us = 0; us < 4; ++us )
  {
    GPUSection gpuSection( commandList, L"Upsample blur bloom" );

    commandList.ChangeResourceState( *bloomTextures[ 3 - us ][ 0 ], ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );
    commandList.ChangeResourceState( *bloomTextures[ 4 - us ][ 1 ], ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );
    commandList.ChangeResourceState( *bloomTextures[ 3 - us ][ 1 ], ResourceStateBits::UnorderedAccess );

    struct
    {
      float invOutputWidth;
      float invOutputHeight;
      float upsampleBlendFactor;
    } upsampleParams;

    upsampleParams.invOutputWidth      = 1.0f / bloomTextures[ 3 - us ][ 0 ]->GetTextureWidth();
    upsampleParams.invOutputHeight     = 1.0f / bloomTextures[ 3 - us ][ 0 ]->GetTextureHeight();
    upsampleParams.upsampleBlendFactor = 0.65f;

    commandList.SetComputeShader( *upsampleBlurBloomShader );
    commandList.SetComputeConstantValues( 0, upsampleParams, 0 );
    commandList.SetComputeShaderResourceView( 1, *bloomTextures[ 3 - us ][ 0 ] );
    commandList.SetComputeShaderResourceView( 2, *bloomTextures[ 4 - us ][ 1 ] );
    commandList.SetComputeUnorderedAccessView( 3, *bloomTextures[ 3 - us ][ 1 ] );
    commandList.Dispatch( ( bloomTextures[ 3 - us ][ 0 ]->GetTextureWidth () + ExtractBloomKernelWidth - 1  ) / ExtractBloomKernelWidth
                        , ( bloomTextures[ 3 - us ][ 0 ]->GetTextureHeight() + ExtractBloomKernelHeight - 1 ) / ExtractBloomKernelHeight
                        , 1 );

    commandList.AddUAVBarrier( { *bloomTextures[ 3 - us ][ 1 ] } );
  }

  {
    GPUSection gpuSection( commandList, L"ToneMapping" );

    commandList.ChangeResourceState( { { *exposureBuffer, ResourceStateBits::VertexOrConstantBuffer }
                                     , { *hqColorTexture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                     , { *bloomTextures[ 0 ][ 1 ], ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                     , { backBuffer, ResourceStateBits::RenderTarget } } );

    struct
    {
      float invTexWidth;
      float invTexHeight;
      float bloomStrength;
    } toneMappingParams;

    toneMappingParams.invTexWidth   = 1.0f / backBuffer.GetTextureWidth();
    toneMappingParams.invTexHeight  = 1.0f / backBuffer.GetTextureHeight();
    toneMappingParams.bloomStrength = bloomStrength;

    commandList.SetRenderTarget( backBuffer, nullptr );

    commandList.SetPipelineState( RenderManager::GetInstance().GetPipelinePreset( PipelinePresets::ToneMapping ) );
    commandList.SetPrimitiveType( PrimitiveType::TriangleList );
    commandList.SetConstantValues( 0, toneMappingParams, 0 );
    commandList.SetConstantBuffer( 1, *exposureBuffer );
    commandList.SetShaderResourceView( 2, *hqColorTexture );
    commandList.SetShaderResourceView( 3, *bloomTextures[ 0 ][ 1 ] );
    commandList.Draw( 3 );
  }
}

void Scene::AdaptExposure( CommandList& commandList )
{
  GPUSection gpuSection( commandList, L"Adapt exposure" );

  static const bool enableAdaptation = false;

  if ( !enableAdaptation )
  {
    GPUSection gpuSection( commandList, L"Manual exposure" );
    InitializeManualExposure( commandList, *exposureBuffer, *exposureOnlyBuffer, manualExposure );
  }
  else
  {
    {
      GPUSection gpuSection( commandList, L"Clear histogram" );

      uint32_t zeroes[ 256 ] = { 0 };
      auto clearHistoUpload = RenderManager::GetInstance().GetUploadBufferForResource( *histogramBuffer );
      commandList.UploadBufferResource( eastl::move( clearHistoUpload ), *histogramBuffer, zeroes, sizeof( zeroes ) );
    }

    {
      GPUSection gpuSection( commandList, L"Build histogram" );

      commandList.ChangeResourceState( { { *lumaTexture, ResourceStateBits::NonPixelShaderInput }
                                       , { *histogramBuffer, ResourceStateBits::UnorderedAccess } } );

      commandList.SetComputeShader( *generateHistogramShader );
      commandList.SetComputeShaderResourceView( 0, *lumaTexture );
      commandList.SetComputeUnorderedAccessView( 1, *histogramBuffer );
      commandList.Dispatch( ( lumaTexture->GetTextureWidth()  + 16  - 1 ) / 16
                          , ( lumaTexture->GetTextureHeight() + 384 - 1 ) / 384
                          , 1 );

      commandList.AddUAVBarrier( { *histogramBuffer } );
    }

    {
      GPUSection gpuSection( commandList, L"Calc new exposure" );

      struct
      {
        float    targetLuminance;
        float    adaptationRate;
        float    minExposure;
        float    maxExposure;
        uint32_t pixelCount;
      } calcExposureParams;

      calcExposureParams.targetLuminance = targetLuminance;
      calcExposureParams.adaptationRate  = adaptationRate;
      calcExposureParams.minExposure     = minExposure;
      calcExposureParams.maxExposure     = maxExposure;
      calcExposureParams.pixelCount      = lumaTexture->GetTextureWidth() * lumaTexture->GetTextureHeight();

      commandList.ChangeResourceState( { { *histogramBuffer, ResourceStateBits::NonPixelShaderInput }
                                       , { *exposureBuffer, ResourceStateBits::UnorderedAccess }
                                       , { *exposureOnlyBuffer, ResourceStateBits::UnorderedAccess } } );

      commandList.SetComputeShader( *adaptExposureShader );
      commandList.SetComputeConstantValues( 0, calcExposureParams, 0 );
      commandList.SetComputeShaderResourceView( 1, *histogramBuffer );
      commandList.SetComputeUnorderedAccessView( 2, *exposureBuffer );
      commandList.SetComputeUnorderedAccessView( 3, *exposureOnlyBuffer );
      commandList.Dispatch( 1, 1, 1 );

      commandList.AddUAVBarrier( { *histogramBuffer, *exposureBuffer, *exposureOnlyBuffer } );

      commandList.ChangeResourceState( { { *exposureBuffer, ResourceStateBits::VertexOrConstantBuffer }
                                       , { *exposureOnlyBuffer, ResourceStateBits::NonPixelShaderInput } } );
    }
  }
}

void Scene::Upscale( CommandList& commandList, Resource& backBuffer )
{
  if ( upscaling )
  {
    GPUSection gpuSection( commandList, L"Upscaling" );

    upscaling->Upscale( commandList
                      , *lqColorTexture
                      , *depthTexture
                      , *motionVectorTexture
                      , *hqColorTexture
                      , *exposureOnlyBuffer
                      , upscaling->GetJitter() );

    commandList.SetViewport( 0, 0, backBuffer.GetTextureWidth(), backBuffer.GetTextureHeight());
    commandList.SetScissor ( 0, 0, backBuffer.GetTextureWidth(), backBuffer.GetTextureHeight());
  }
}

void Scene::RenderDebugLayer( CommandList& commandList, DebugOutput debugOutput )
{
  if ( debugOutput == DebugOutput::None )
    return;

  auto& manager = RenderManager::GetInstance();

  struct
  {
    XMFLOAT2    leftTop;
    XMFLOAT2    widthHeight;
    uint32_t    mipLevel;
    uint32_t    sceneTextureId;
    DebugOutput debugOutput;
  } quadParams;

  quadParams.leftTop.x      = -1;
  quadParams.leftTop.y      = 1;
  quadParams.widthHeight.x  = 2;
  quadParams.widthHeight.y  = -2;
  quadParams.mipLevel       = 0;
  quadParams.sceneTextureId = 0xFFFFFFFFU;
  quadParams.debugOutput    = debugOutput;

  commandList.SetPipelineState( manager.GetPipelinePreset( PipelinePresets::QuadDebug ) );
  commandList.SetVertexBufferToNull();
  commandList.SetIndexBufferToNull();
  commandList.SetPrimitiveType( PrimitiveType::TriangleStrip );
  commandList.SetConstantValues( 0, quadParams, 0 );

  Resource* texture = nullptr;
  switch ( debugOutput )
  {
  case DebugOutput::Denoiser:           texture = denoiserValidationTexture;  break;
  case DebugOutput::AO:                 texture = aoTexture.get(); break;
  case DebugOutput::DenoisedAO:         texture = denoisedAOTexture; break;
  case DebugOutput::Shadow:             texture = shadowTexture.get(); break;
  case DebugOutput::DenoisedShadow:     texture = denoisedShadowTexture; break;
  case DebugOutput::Reflection:         texture = reflectionTexture.get(); break;
  case DebugOutput::DenoisedReflection: texture = denoisedReflectionTexture; break;
  case DebugOutput::GI:                 texture = giTexture.get(); break;
  case DebugOutput::DenoisedGI:         texture = denoisedGITexture; break;
  }
  assert( texture );
  commandList.ChangeResourceState( *texture, ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );
  commandList.SetShaderResourceView( 1, *texture );
  commandList.Draw( 4 );
}

void Scene::Render( CommandAllocator& commandAllocator
                  , CommandList& commandList
                  , Resource& backBuffer
                  , bool useTextureFeedback
                  , bool freezeCulling
                  , DebugOutput debugOutput )
{
  auto& renderManager = RenderManager::GetInstance();
  auto& device        = renderManager.GetDevice();

  auto& renderTarget = upscaling ? *lqColorTexture : *hqColorTexture;

  XMFLOAT2 jitter = XMFLOAT2( 0, 0 );
  if ( upscaling )
    jitter = upscaling->GetJitter();

  GPUSection gpuSection( commandList, L"Render scene" );

  CullScene( commandList, jitter.x, jitter.y, renderTarget.GetTextureWidth(), renderTarget.GetTextureHeight(), useTextureFeedback, freezeCulling );

  // For the very fist frame, we run culling twice. This is because in culling, we are using prev frame VP transform for culling. So with the
  // first run, we make the current frame matrix, and copy it to prev in the next one.
  if ( frameCounter == 0 )
    CullScene( commandList, jitter.x, jitter.y, renderTarget.GetTextureWidth(), renderTarget.GetTextureHeight(), useTextureFeedback, freezeCulling );

  RenderSkyToCube( commandList );
  ClearTexturesAndPrepareRendering( commandList, renderTarget );

  commandList.SetViewport( 0, 0, renderTarget.GetTextureWidth(), renderTarget.GetTextureHeight() );
  commandList.SetScissor( 0, 0, renderTarget.GetTextureWidth(), renderTarget.GetTextureHeight() );

  RenderDepth( commandList );
  RenderShadow( commandList );
  RenderGI( commandList );
  RenderAO( commandList );
  RenderReflection( commandList );
  Denoise( commandAllocator, commandList, jitter.x, jitter.y, debugOutput == DebugOutput::Denoiser );
  commandList.SetRenderTarget( renderTarget, nullptr );
  RenderDirectLighting( commandList );
  commandList.SetRenderTarget( renderTarget, depthTexture.get() );
  RenderSky( commandList );
  RenderTranslucent( commandList );
  Upscale( commandList, backBuffer );
  PostProcessing( commandList, backBuffer );
  AdaptExposure( commandList );

  RenderDebugLayer( commandList, debugOutput );

  ++frameCounter;
}

static Node* FindNodeByName( const char* name, Node& node )
{
  if ( strcmp( node.GetName(), name ) == 0 )
    return &node;

  Node* result = nullptr;
  node.ForEachNode( [&]( Node& childNode ) mutable
  {
    result = FindNodeByName( name, childNode );
    return !result;
  } );

  return result;
}

Node* Scene::FindNodeByName( const char* name )
{
  return ::FindNodeByName( name, *rootNode );
}

void Scene::OnNodeTransformChanged( CommandList& commandList, const Node& node )
{
  auto& manager = RenderManager::GetInstance();

  auto nodeIter = nodeToIndex.find( &node );
  assert( nodeIter != nodeToIndex.end() );

  auto  nodeIndex = nodeIter->second;
  auto& nodeSlot  = nodeSlots[ nodeIndex ];

  XMStoreFloat4x4( &nodeSlot.worldTransform, node.GetTransform() );

  commandList.UpdateBufferRegion( CreateBufferFromData( &nodeSlot, 1, ResourceType::Buffer, manager.GetDevice(), commandList, L"nodeChange" ), *nodeBuffer, sizeof( NodeSlot ) * nodeIndex );
}
