// BVH accelerated ray tracer.

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

struct TextureInfo {
    int  index;
    int  texCoordSet;
    int2 pad_0;
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

struct Material {
    float4               emissiveFactor;
    PbrMetallicRoughness pbr;
    TextureInfo          emissiveTexture;
};

struct DirectionalLight {
    float4 color;
    float4 direction;
    float  intensity;
    float  range;
    float2 pad_0;
};

struct PointLight {
    float4 position;
    float4 color;
    float  intensity;
    float  range;
    float2 pad_0;
};

struct SpotLight {
    float4 position;
    float4 color;
    float4 direction;
    float  intensity;
    float  range;

    float innerConeAngle;
    float outerConeAngle;
};


struct CameraData {
    float4x4 viewInverse;
    float4x4 projInverse;
    float3   position;
    float    pad_0;
};

struct SceneSettings {
    CameraData camera;

    uint vertexCount;
    uint triangleCount;

    uint emissiveLightCount;
    uint directionalLightCount;
    uint pointLightCount;
    uint spotLightCount;

    uint maxBounces;
    uint samplesPerPixel;
    uint samplesPerEmissiveLight;

    uint3 pad_0;
};

// Each node packs the AABB and two uint fields into two float4s.
// aabbMinLeft.xyz  = AABB min corner
// aabbMinLeft.w    = asuint → leftFirst  (leaf: first index in bvhTriIndices;
//                                         internal: left child index, right = leftFirst+1)
// aabbMaxCount.xyz = AABB max corner
// aabbMaxCount.w   = asuint → triCount   (0 = internal node, >0 = leaf)
struct BVHNode {
    float4 aabbMinLeft;
    float4 aabbMaxCount;
};

[vk::binding(0, 0)]  RWTexture2D<float4>                outputImage;
[vk::binding(1, 0)]  StructuredBuffer<Vertex>           vertices;
[vk::binding(2, 0)]  StructuredBuffer<Triangle>         triangles;
[vk::binding(3, 0)]  StructuredBuffer<Material>         materials;
[vk::binding(4, 0)]  StructuredBuffer<uint>             emissiveLight;
[vk::binding(5, 0)]  StructuredBuffer<DirectionalLight> directionalLight;
[vk::binding(6, 0)]  StructuredBuffer<PointLight>       pointLight;
[vk::binding(7, 0)]  StructuredBuffer<SpotLight>        spotLight;
[vk::binding(8, 0)]  ConstantBuffer<SceneSettings>      scene;
[vk::binding(9, 0)]  RWStructuredBuffer<float4>         accumBuffer;
[vk::binding(10, 0)] StructuredBuffer<BVHNode>          bvhNodes;
[vk::binding(11, 0)] StructuredBuffer<uint>             bvhTriIndices;
[vk::binding(12, 0)] Sampler2D                          textures[];

struct PushConstants {
    uint sampleIndex;
    uint tileOffsetX;
    uint tileOffsetY;
};
[[vk::push_constant]] PushConstants pushConsts;

static const float FUZZ = 0;
static const float EPS = 0.001;
static const float PI  =  3.1415926535897932385;

struct Ray {
    float3 origin;
    float3 direction;
};

float3 rayAt(Ray ray, float t) {
    return ray.origin + t * ray.direction;
}

struct HitInfo {
    float  t;
    float3 position;
    float3 normal;
    float2 texCoord;
    float2 mrCoord;
    uint   materialIndex;
    bool   hit;
    bool   front_face;
};

float4 getBaseColor(Material mat, HitInfo hit) {
    float4 baseColor = mat.pbr.baseColorFactor;
    if (mat.pbr.baseColorTexture.index >= 0) {
        baseColor = textures[mat.pbr.baseColorTexture.index].SampleLevel(hit.texCoord, 0);
    }
    return baseColor;
}

float4 getMetallicRoughness(Material mat, HitInfo hit) {
    float4 result;
    if (mat.pbr.metallicRoughnessTexture.index >= 0) {
        result = textures[mat.pbr.metallicRoughnessTexture.index].SampleLevel(hit.mrCoord, 0);
    } else {
        result.b = mat.pbr.metallicFactor;
        result.g = mat.pbr.roughnessFactor;
    }

    return result;
}

float linear_to_gamma(float linear_component) {
    if (linear_component > 0)
        return sqrt(linear_component);

    return 0;
}
bool near_zero(float3 e) {
    return all(abs(e) < 1e-8);
}

float3 reflect(float3 v, float3 n) {
    return v - 2*dot(v,n)*n;
}
// random

uint hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

struct RNG {
    uint state;
};

void rng_init(inout RNG rng, uint2 pixelCoord, uint frameIndex) {
    rng.state = hash(pixelCoord.x + hash(pixelCoord.y + hash(frameIndex)));
}

float rng_next(inout RNG rng) {
    rng.state = hash(rng.state);
    return float(rng.state) / 4294967296.0;
}

float2 rng_next2(inout RNG rng) {
    return float2(rng_next(rng), rng_next(rng));
}

float3 rng_next3(inout RNG rng) {
    return float3(rng_next(rng), rng_next(rng), rng_next(rng));
}

float3 random_unit_vector(inout RNG rng) {
    while (true) {
        float3 p = rng_next3(rng) * 2.0 - 1.0;
        float lensq = dot(p, p);

        if (lensq > 1e-6 && lensq <= 1.0)
            return p;
    }
}

float3 random_on_hemisphere(float3 normal, inout RNG rng) {
    float3 on_unit_sphere = normalize(random_unit_vector(rng));
    if (dot(on_unit_sphere, normal) > 0.0) {
        return on_unit_sphere;
    } else {
        return -on_unit_sphere;
    }
}

// rays

void set_face_normal(Ray r, inout HitInfo hitInfo) {
    hitInfo.front_face = dot(r.direction, hitInfo.normal) < 0;
    hitInfo.normal = hitInfo.front_face ? hitInfo.normal : -hitInfo.normal;
}

Ray generateEyeRay(uint2 pixelCoord, uint2 dimensions, inout RNG rng) {
    float2 uv = (float2(pixelCoord) + rng_next2(rng)) / float2(dimensions);
    uv = uv * 2.0 - 1.0;

    float4 target = mul(scene.camera.projInverse, float4(uv.x, -uv.y, 1, 1));
    float4 direction = mul(scene.camera.viewInverse, float4(normalize(target.xyz / target.w), 0));

    Ray ray;
    ray.origin = scene.camera.position;
    ray.direction = normalize(direction.xyz);

    return ray;
}

bool intersectTriangle(Ray ray, Triangle tri, out float t, out float2 barycentrics) {
    t = 0;
    barycentrics = 0;

    Vertex v0 = vertices[tri.indices[0]];
    Vertex v1 = vertices[tri.indices[1]];
    Vertex v2 = vertices[tri.indices[2]];

    float3 edge1 = v1.position.xyz - v0.position.xyz;
    float3 edge2 = v2.position.xyz - v0.position.xyz;

    float3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);

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

// ── AABB slab test ────────────────────────────────────────────────────────────
// Returns true if ray hits [bmin, bmax] before tMax.
bool intersectAABB(Ray ray, float3 bmin, float3 bmax, float tMax) {
    float3 invDir = 1.0 / ray.direction;
    float3 t0     = (bmin - ray.origin) * invDir;
    float3 t1     = (bmax - ray.origin) * invDir;
    float  tNear  = max(max(min(t0.x, t1.x), min(t0.y, t1.y)), min(t0.z, t1.z));
    float  tFar   = min(min(max(t0.x, t1.x), max(t0.y, t1.y)), max(t0.z, t1.z));
    return tNear <= tFar && tFar > 0.0 && tNear < tMax;
}

// Fill HitInfo after a confirmed triangle hit.
void resolveHit(uint triIdx, float t, float2 bary, Ray ray, inout HitInfo hit) {
    Triangle tri = triangles[triIdx];
    Vertex   v0  = vertices[tri.indices[0]];
    Vertex   v1  = vertices[tri.indices[1]];
    Vertex   v2  = vertices[tri.indices[2]];

    float w = 1.0 - bary.x - bary.y;

    hit.normal = normalize(
        v0.normal.xyz * w +
        v1.normal.xyz * bary.x +
        v2.normal.xyz * bary.y
    );
    set_face_normal(ray, hit);

    hit.hit           = true;
    hit.t             = t;
    hit.position      = ray.origin + ray.direction * t;
    hit.materialIndex = tri.material;

    Material mat = materials[hit.materialIndex];
    hit.texCoord =
        v0.texCoord[mat.pbr.baseColorTexture.texCoordSet].xy * w +
        v1.texCoord[mat.pbr.baseColorTexture.texCoordSet].xy * bary.x +
        v2.texCoord[mat.pbr.baseColorTexture.texCoordSet].xy * bary.y;

    if (mat.pbr.metallicRoughnessTexture.index >= 0) {
        hit.mrCoord =
            v0.texCoord[mat.pbr.metallicRoughnessTexture.texCoordSet].xy * w +
            v1.texCoord[mat.pbr.metallicRoughnessTexture.texCoordSet].xy * bary.x +
            v2.texCoord[mat.pbr.metallicRoughnessTexture.texCoordSet].xy * bary.y;
    }
}

HitInfo traceScene(inout Ray ray) {
    HitInfo hit;
    hit.hit = false;
    hit.t   = 1e10;

    uint stack[64];
    int  top  = 0;
    stack[0]  = 0; // root

    while (top >= 0) {
        BVHNode node     = bvhNodes[stack[top--]];
        uint    leftFirst = asuint(node.aabbMinLeft.w);
        uint    triCount  = asuint(node.aabbMaxCount.w);

        if (!intersectAABB(ray, node.aabbMinLeft.xyz, node.aabbMaxCount.xyz, hit.t))
            continue;

        if (triCount > 0u) {
            // Leaf: test every primitive
            for (uint i = 0u; i < triCount; i++) {
                uint   triIdx = bvhTriIndices[leftFirst + i];
                float  t;
                float2 bary;
                if (intersectTriangle(ray, triangles[triIdx], t, bary) && t < hit.t)
                    resolveHit(triIdx, t, bary, ray, hit);
            }
        } else {
            // Internal: push both children (right child = leftFirst + 1)
            if (top < 62) {
                stack[++top] = leftFirst;
                stack[++top] = leftFirst + 1u;
            }
        }
    }

    return hit;
}

bool metall_scatter(Ray r_in, HitInfo hit, Material mat, inout RNG rng, inout float3 attenuation, inout Ray scattered) {
    float3 reflectedDir = reflect(r_in.direction, hit.normal);
    reflectedDir = normalize(reflectedDir) + (FUZZ * random_on_hemisphere(hit.normal, rng));
    scattered.origin = hit.position + hit.normal * 1e-4;
    scattered.direction = reflectedDir;
    attenuation = getBaseColor(mat, hit).xyz;
    return true;
}

bool lambertian_scatter(Ray r_in, HitInfo hit, Material mat, inout RNG rng, inout float3 attenuation, inout Ray scattered) {
    float3 scatter_direction  = normalize(hit.normal + random_on_hemisphere(hit.normal, rng));

    if (near_zero(scatter_direction)) {
        scatter_direction = hit.normal;
    }

    scattered.origin = hit.position + hit.normal * 1e-4;
    scattered.direction = scatter_direction;
    attenuation = getBaseColor(mat, hit).xyz;
    return true;
}

bool scatter(Ray r_in, HitInfo hit, inout RNG rng, inout float3 attenuation, inout Ray scattered, out bool isSpecular) {
    Material mat = materials[hit.materialIndex];
    float4 mrFactor = getMetallicRoughness(mat, hit);
    if (mrFactor.b > 0.5) {
        isSpecular = true;
        return metall_scatter(r_in, hit, mat, rng, attenuation, scattered);
    } else {
        isSpecular = false;
        return lambertian_scatter(r_in, hit, mat, rng, attenuation, scattered);
    }
}

// ── BVH shadow ray (any-hit, returns early) ───────────────────────────────────
bool isOccluded(float3 origin, float3 dir, float maxDist, uint skipTriIdx) {
    Ray shadowRay;
    shadowRay.origin    = origin;
    shadowRay.direction = dir;

    uint stack[64];
    int  top = 0;
    stack[0] = 0;

    while (top >= 0) {
        BVHNode node      = bvhNodes[stack[top--]];
        uint    leftFirst = asuint(node.aabbMinLeft.w);
        uint    triCount  = asuint(node.aabbMaxCount.w);

        if (!intersectAABB(shadowRay, node.aabbMinLeft.xyz, node.aabbMaxCount.xyz, maxDist))
            continue;

        if (triCount > 0u) {
            for (uint i = 0u; i < triCount; i++) {
                uint triIdx = bvhTriIndices[leftFirst + i];
                if (triIdx == skipTriIdx)
                    continue;
                float  t;
                float2 bc;
                if (intersectTriangle(shadowRay, triangles[triIdx], t, bc) && t < maxDist - 1e-4)
                    return true;
            }
        } else {
            if (top < 62) {
                stack[++top] = leftFirst;
                stack[++top] = leftFirst + 1u;
            }
        }
    }
    return false;
}

// Sample a random point on emissive triangle at list index listIdx
float3 sampleEmissiveTriangle(uint listIdx, inout RNG rng, out float3 lightNormal, out float area) {
    uint     triIdx = emissiveLight[listIdx];
    Triangle tri    = triangles[triIdx];
    Vertex   v0     = vertices[tri.indices[0]];
    Vertex   v1     = vertices[tri.indices[1]];
    Vertex   v2     = vertices[tri.indices[2]];

    float r1 = rng_next(rng);
    float r2 = rng_next(rng);
    if (r1 + r2 > 1.0) { r1 = 1.0 - r1; r2 = 1.0 - r2; }
    float w = 1.0 - r1 - r2;

    float3 e1       = v1.position.xyz - v0.position.xyz;
    float3 e2       = v2.position.xyz - v0.position.xyz;
    float3 crossVec = cross(e1, e2);
    area            = 0.5 * length(crossVec);
    lightNormal     = normalize(crossVec);

    return v0.position.xyz * w + v1.position.xyz * r1 + v2.position.xyz * r2;
}

// Direct light sampling (NEE) contribution at a diffuse hit point
float3 estimateDirect(HitInfo hit, Material mat, inout RNG rng) {
    uint  lightIdx = min(uint(rng_next(rng) * float(scene.emissiveLightCount)), scene.emissiveLightCount - 1);

    float3 lightNormal;
    float  area;
    float3 lightPos = sampleEmissiveTriangle(lightIdx, rng, lightNormal, area);

    float3 toLight    = lightPos - hit.position;
    float  dist       = length(toLight);
    float3 toLightDir = toLight / dist;

    float cosTheta      = dot(hit.normal,   toLightDir);
    float cosThetaLight = abs(dot(lightNormal, toLightDir));

    if (cosTheta <= 0.0)
        return float3(0.0, 0.0, 0.0);

    uint   lightTriIdx  = emissiveLight[lightIdx];
    float3 shadowOrigin = hit.position + hit.normal * 1e-4;
    if (isOccluded(shadowOrigin, toLightDir, dist, lightTriIdx))
        return float3(0.0, 0.0, 0.0);

    Material lightMat = materials[triangles[lightTriIdx].material];
    float3   Le       = lightMat.emissiveFactor.rgb;
    float3   albedo   = getBaseColor(mat, hit).xyz;
    float    pdf      = (dist * dist) / (area * cosThetaLight * float(scene.emissiveLightCount));

    return Le * (albedo / PI) * cosTheta / pdf;
}

float4 ray_color(Ray r, inout RNG rng) {
    float3 throughput     = float3(1.0, 1.0, 1.0);
    float3 pixelColor     = float3(0.0, 0.0, 0.0);
    bool   specularBounce = true;

    for (int bounce = 0; bounce < scene.maxBounces; ++bounce) {
        HitInfo hit = traceScene(r);
        if (!hit.hit) {
            if (specularBounce)
                   pixelColor += throughput * float3(0, 0, 0);
            break;
        }

        Material mat          = materials[hit.materialIndex];
        float3   emittedColor = mat.emissiveFactor.rgb;

        if (specularBounce)
            pixelColor += throughput * emittedColor;

        Ray    scattered;
        float3 attenuation;
        bool   isSpecular;

        if (!scatter(r, hit, rng, attenuation, scattered, isSpecular))
            break;

        if (!isSpecular && scene.emissiveLightCount > 0)
            pixelColor += throughput * estimateDirect(hit, mat, rng);

        throughput    *= attenuation;
        r              = scattered;
        specularBounce = isSpecular;
    }

    return float4(pixelColor, 1.0);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 dimensions;
    outputImage.GetDimensions(dimensions.x, dimensions.y);

    uint2 pixelCoord = dispatchThreadID.xy + uint2(pushConsts.tileOffsetX, pushConsts.tileOffsetY);

    if (pixelCoord.x >= dimensions.x || pixelCoord.y >= dimensions.y)
        return;

    uint pixelIndex = pixelCoord.y * dimensions.x + pixelCoord.x;

    RNG rng;
    rng_init(rng, pixelCoord, pushConsts.sampleIndex);
    Ray ray = generateEyeRay(pixelCoord, dimensions, rng);
    float4 color = ray_color(ray, rng);

    if (pushConsts.sampleIndex == 0) {
        accumBuffer[pixelIndex] = color;
    } else {
        accumBuffer[pixelIndex] += color;
    }

    if (pushConsts.sampleIndex == scene.samplesPerPixel - 1) {
        float4 finalColor = accumBuffer[pixelIndex] / float(scene.samplesPerPixel);
        finalColor.x = linear_to_gamma(finalColor.x);
        finalColor.y = linear_to_gamma(finalColor.y);
        finalColor.z = linear_to_gamma(finalColor.z);
        outputImage[pixelCoord] = finalColor;
    }
}
