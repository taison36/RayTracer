struct Vertex {
    float4 position;
    float4 normal;
    float4 tangent;
    float4 texCoord[16];
};

struct Triangle {
    uint indices[3];
    uint material;
};

struct TextureInfo
{
    int index;    
    int texCoordSet;
};

struct PbrMetallicRoughness {
    float4      baseColorFactor;
    TextureInfo baseColorTexture;
    float       metallicFactor;
    float       roughnessFactor;
    float2      _pad0;

    TextureInfo metallicRoughnessTexture;
    float4      _pad1;
};

struct Material
{
    float4               emissiveFactor;
    PbrMetallicRoughness pbr;
};

struct CameraData
{
    float4x4 viewInverse;
    float4x4 projInverse;
    float3   position;
};

struct SceneSettings {
    CameraData camera;
    uint       vertexCount;
    uint       triangleCount;
    uint       maxBounces;
    uint       samplesPerPixel;
};

// -----------------------------------------------------------
// Bindings
// -----------------------------------------------------------
[vk::binding(0, 0)] RWTexture2D<float4>           outputImage;
[vk::binding(1, 0)] StructuredBuffer<Vertex>      vertices;
[vk::binding(2, 0)] StructuredBuffer<Triangle>    triangles;
[vk::binding(3, 0)] StructuredBuffer<Material>    materials;
[vk::binding(4, 0)] ConstantBuffer<SceneSettings> scene;
[vk::binding(5, 0)] Sampler2D                     textures[];

// -----------------------------------------------------------
// Ray Structure
// -----------------------------------------------------------
struct Ray {
    float3 origin;
    float3 direction;
};

struct HitInfo {
    bool   hit;
    float  t;
    float3 position;
    float3 normal;
    float2 texCoord;
    uint   materialIndex;
};

// -----------------------------------------------------------
// Ray Generation
// -----------------------------------------------------------
Ray generateRay(uint2 pixelCoord, uint2 dimensions) {
    // Normalized device coordinates [-1, 1]
    float2 ndc = (float2(pixelCoord) + 0.5) / float2(dimensions) * 2.0 - 1.0;
    
    // Unproject to get ray direction
    float4 target = mul(scene.camera.projInverse, float4(ndc.x, ndc.y, 1.0, 1.0));
    target /= target.w;
    
    float3 direction = normalize(mul(scene.camera.viewInverse, float4(target.xyz, 0.0)).xyz);
    
    Ray ray;
    ray.origin = scene.camera.position;
    ray.direction = direction;
    return ray;
}

// -----------------------------------------------------------
// Ray-Triangle Intersection (Möller–Trumbore algorithm)
// -----------------------------------------------------------
bool intersectTriangle(Ray ray, uint triIndex, out float t, out float2 barycentrics)
{
    t = 0;
    barycentrics = 0;

    Triangle tri = triangles[triIndex];
    
    Vertex v0 = vertices[tri.indices[0]];
    Vertex v1 = vertices[tri.indices[1]];
    Vertex v2 = vertices[tri.indices[2]];
    
    float3 edge1 = v1.position.xyz - v0.position.xyz;
    float3 edge2 = v2.position.xyz - v0.position.xyz;
    
    float3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);
    
    // Ray parallel to triangle
    if (abs(a) < 1e-8)
        return false;
    
    float f = 1.0 / a;
    float3 s = ray.origin - v0.position.xyz;
    float u = f * dot(s, h);
    
    if (u < 0.0 || u > 1.0)
        return false;
    
    float3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);
    
    if (v < 0.0 || u + v > 1.0)
        return false;
    
    t = f * dot(edge2, q);
    
    if (t < 1e-6)
        return false;
    
    barycentrics = float2(u, v);
    return true;
}

// -----------------------------------------------------------
// Scene Intersection
// -----------------------------------------------------------
HitInfo traceRay(Ray ray) {
    HitInfo hit;
    hit.hit = false;
    hit.t = 1e10;
    
    for (uint i = 0; i < scene.triangleCount; i++)
    {
        float t;
        float2 barycentrics;
        
        if (intersectTriangle(ray, i, t, barycentrics))
        {
            if (t < hit.t)
            {
                Triangle tri = triangles[i];
                
                Vertex v0 = vertices[tri.indices[0]];
                Vertex v1 = vertices[tri.indices[1]];
                Vertex v2 = vertices[tri.indices[2]];
                
                float w = 1.0 - barycentrics.x - barycentrics.y;
                
                // Interpolate normal
                hit.normal = normalize(
                    v0.normal.xyz * w +
                    v1.normal.xyz * barycentrics.x +
                    v2.normal.xyz * barycentrics.y
                );

                hit.hit = true;
                hit.t = t;
                hit.position = ray.origin + ray.direction * t;
                hit.materialIndex = tri.material;
                Material mat = materials[hit.materialIndex];
                
                hit.texCoord = 
                    v0.texCoord[mat.pbr.baseColorTexture.texCoordSet].xy * w +
                    v1.texCoord[mat.pbr.baseColorTexture.texCoordSet].xy * barycentrics.x +
                    v2.texCoord[mat.pbr.baseColorTexture.texCoordSet].xy * barycentrics.y;
            }
        }
    }
    
    return hit;
}

float4 shade(HitInfo hit)
{
    if (!hit.hit)
    {
        // Sky/background color
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    
    Material mat = materials[hit.materialIndex];

    int texIndex = mat.pbr.baseColorTexture.index;
    return texIndex != -1 ? textures[texIndex].SampleLevel(hit.texCoord, 0.0) : mat.pbr.baseColorFactor;
}
// -----------------------------------------------------------
// Compute Shader Entry Point
// -----------------------------------------------------------
[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;
    uint2 dimensions;
    outputImage.GetDimensions(dimensions.x, dimensions.y);
    
    if (pixelCoord.x >= dimensions.x || pixelCoord.y >= dimensions.y)
        return;
    
    // Generate primary ray
    Ray ray = generateRay(pixelCoord, dimensions);
    
    // Trace ray
    HitInfo hit = traceRay(ray);
    
    // Shade
    float4 color = shade(hit);
    
    // Write to output
    outputImage[pixelCoord] = color;
}
