struct Vertex
{
    float3 position;
    float3 normal;
    float4 tangent;
    float2 texCoord[16];
};

struct Triangle
{
    uint indices[3];
    uint material;
};

struct TextureInfo
{
    int index;    
    int texCoordSet; // which UV set to use
};

struct PbrMetallicRoughness
{
    float4      baseColorFactor;
    TextureInfo baseColorTexture;
    float       metallicFactor;
    float       roughnessFactor;
    TextureInfo metallicRoughnessTexture;
};

struct Material
{
    float3               emissiveFactor;
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
    uint       vertexCount                    ;
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

//utils types
struct Ray {
    float3 origin;
    float3 dir;
};

Ray generateRay(uint2 pixel, uint2 dims, CameraData cam)
{
    float2 uv = (float2(pixel) + 0.5) / float2(dims);
    uv = uv * 2.0 - 1.0;

    float4 clip = float4(uv, 1.0, 1.0);

    float4 view = mul(cam.projInverse, clip);
    view.xyz /= view.w;

    float4 world = mul(cam.viewInverse, float4(view.xyz, 0.0));

    Ray ray;
    ray.origin = cam.position;
    ray.dir = normalize(world.xyz);
    return ray;
}

bool intersectTriangle(Ray ray, float3 v0, float3 v1, float3 v2, out float t)
{
    t = 0;
    const float EPS = 1e-6;

    float3 e1 = v1 - v0;
    float3 e2 = v2 - v0;

    float3 p = cross(ray.dir, e2);
    float det = dot(e1, p);

    if (abs(det) < EPS)
        return false;

    float invDet = 1.0 / det;

    float3 s = ray.origin - v0;
    float u = dot(s, p) * invDet;
    if (u < 0.0 || u > 1.0)
        return false;

    float3 q = cross(s, e1);
    float v = dot(ray.dir, q) * invDet;
    if (v < 0.0 || (u + v) > 1.0)
        return false;

    t = dot(e2, q) * invDet;
    return t > EPS;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 DTid : SV_DispatchThreadID)
{
    uint2 dims;
    outputImage.GetDimensions(dims.x, dims.y);

    if (any(DTid.xy >= dims))
        return;

    uint2 pixel = DTid.xy;

    Ray ray = generateRay(pixel, dims, scene.camera);

    bool hit = false;

    // brute force scene intersection
    for (uint i = 0; i < scene.triangleCount; i++)
    {
        Triangle tri = triangles[i];

        float3 v0 = vertices[tri.indices[0]].position;
        float3 v1 = vertices[tri.indices[1]].position;
        float3 v2 = vertices[tri.indices[2]].position;

        float t;
        if (intersectTriangle(ray, v0, v1, v2, t))
        {
            hit = true;
            break;
        }
    }
    //pixel.y = dims.y - 1 - pixel.y;

    if (hit)
        outputImage[pixel] = float4(0, 0, 0, 1);
    else
        outputImage[pixel] = float4(1, 1, 1, 1);
}
