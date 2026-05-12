// KD-Tree accelerated ray tracer.
// Identical to bvh.slang in materials/shading/lighting. The only difference is
// that traceScene() and isOccluded() use ordered KD-Tree traversal instead of
// BVH traversal. Ordered traversal tracks the [tMin,tMax] ray interval per
// stack entry and visits the near child first, enabling early exit as soon as
// the front of the stack is farther than the current best hit.

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
    float  innerConeAngle;
    float  outerConeAngle;
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

// ── KD-Tree node (32 bytes) ───────────────────────────────────────────────────
// data0.xyz  = AABB min
// data0.w    = splitPos as float (interior) │ bit_cast<float>(firstPrim) (leaf)
// data1.xyz  = AABB max
// data1.w    = bit_cast<float>(flags):
//                bit  31     : 1 = leaf, 0 = interior
//                bits 30-29  : split axis (0=X, 1=Y, 2=Z)  [interior only]
//                bits 0-28   : leftChild index              [interior]
//                            │ triCount                     [leaf]
//              right child is always leftChild + 1
struct KDNode {
    float4 data0;
    float4 data1;
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
[vk::binding(10, 0)] StructuredBuffer<KDNode>           kdNodes;
[vk::binding(11, 0)] StructuredBuffer<uint>             kdTriIndices;
[vk::binding(12, 0)] Sampler2D                          textures[]; // must be last (unbounded)

struct PushConstants {
    uint sampleIndex;
    uint tileOffsetX;
    uint tileOffsetY;
};
[[vk::push_constant]] PushConstants pushConsts;

static const float FUZZ = 0;
static const float EPS  = 0.001;
static const float PI   = 3.1415926535897932385;

struct Ray {
    float3 origin;
    float3 direction;
};

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

// util functions
float4 getBaseColor(Material mat, HitInfo hit) {
    float4 baseColor = mat.pbr.baseColorFactor;
    if (mat.pbr.baseColorTexture.index >= 0)
        baseColor = textures[mat.pbr.baseColorTexture.index].SampleLevel(hit.texCoord, 0);
    return baseColor;
}

float4 getMetallicRoughness(Material mat, HitInfo hit) {
    float4 result;
    if (mat.pbr.metallicRoughnessTexture.index >= 0)
        result = textures[mat.pbr.metallicRoughnessTexture.index].SampleLevel(hit.mrCoord, 0);
    else {
        result.b = mat.pbr.metallicFactor;
        result.g = mat.pbr.roughnessFactor;
    }
    return result;
}

float linear_to_gamma(float x) { return x > 0 ? sqrt(x) : 0; }
bool  near_zero(float3 e)      { return all(abs(e) < 1e-8); }
float3 reflect(float3 v, float3 n) { return v - 2 * dot(v, n) * n; }

// random
uint hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

struct RNG { uint state; };

void rng_init(inout RNG rng, uint2 pixelCoord, uint frameIndex) {
    rng.state = hash(pixelCoord.x + hash(pixelCoord.y + hash(frameIndex)));
}
float  rng_next(inout RNG rng)  { rng.state = hash(rng.state); return float(rng.state) / 4294967296.0; }
float2 rng_next2(inout RNG rng) { return float2(rng_next(rng), rng_next(rng)); }
float3 rng_next3(inout RNG rng) { return float3(rng_next(rng), rng_next(rng), rng_next(rng)); }

float3 random_unit_vector(inout RNG rng) {
    while (true) {
        float3 p    = rng_next3(rng) * 2.0 - 1.0;
        float  lsq  = dot(p, p);
        if (lsq > 1e-6 && lsq <= 1.0) return p;
    }
}

float3 random_on_hemisphere(float3 normal, inout RNG rng) {
    float3 v = normalize(random_unit_vector(rng));
    return dot(v, normal) > 0.0 ? v : -v;
}

void set_face_normal(Ray r, inout HitInfo h) {
    h.front_face = dot(r.direction, h.normal) < 0;
    h.normal     = h.front_face ? h.normal : -h.normal;
}

Ray generateEyeRay(uint2 pixelCoord, uint2 dimensions, inout RNG rng) {
    float2 uv  = (float2(pixelCoord) + rng_next2(rng)) / float2(dimensions);
    uv         = uv * 2.0 - 1.0;
    float4 tgt = mul(scene.camera.projInverse, float4(uv.x, -uv.y, 1, 1));
    float4 dir = mul(scene.camera.viewInverse, float4(normalize(tgt.xyz / tgt.w), 0));
    Ray ray;
    ray.origin    = scene.camera.position;
    ray.direction = normalize(dir.xyz);
    return ray;
}

bool intersectTriangle(Ray ray, Triangle tri, out float t, out float2 barycentrics) {
    t = 0; barycentrics = 0;
    Vertex v0 = vertices[tri.indices[0]];
    Vertex v1 = vertices[tri.indices[1]];
    Vertex v2 = vertices[tri.indices[2]];
    float3 e1 = v1.position.xyz - v0.position.xyz;
    float3 e2 = v2.position.xyz - v0.position.xyz;
    float3 h  = cross(ray.direction, e2);
    float  a  = dot(e1, h);
    if (abs(a) < 1e-8) return false;
    float  f  = 1.0 / a;
    float3 s  = ray.origin - v0.position.xyz;
    float  u  = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return false;
    float3 q  = cross(s, e1);
    float  v  = f * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;
    t = f * dot(e2, q);
    if (t < 1e-6) return false;
    barycentrics = float2(u, v);
    return true;
}

// AABB slab test returning the [tNear, tFar] interval.
// Returns false if the ray misses or the interval is behind the ray.
bool intersectAABBInterval(Ray ray, float3 bmin, float3 bmax,
                           out float tNear, out float tFar) {
    float3 invDir = 1.0 / ray.direction;
    float3 t0     = (bmin - ray.origin) * invDir;
    float3 t1     = (bmax - ray.origin) * invDir;
    tNear = max(max(min(t0.x, t1.x), min(t0.y, t1.y)), min(t0.z, t1.z));
    tFar  = min(min(max(t0.x, t1.x), max(t0.y, t1.y)), max(t0.z, t1.z));
    return tNear <= tFar && tFar > 0.0;
}

void resolveHit(uint triIdx, float t, float2 bary, Ray ray, inout HitInfo hit) {
    Triangle tri = triangles[triIdx];
    Vertex   v0  = vertices[tri.indices[0]];
    Vertex   v1  = vertices[tri.indices[1]];
    Vertex   v2  = vertices[tri.indices[2]];
    float w = 1.0 - bary.x - bary.y;

    hit.normal = normalize(v0.normal.xyz * w + v1.normal.xyz * bary.x + v2.normal.xyz * bary.y);
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

// ── KD-Tree ordered traversal ─────────────────────────────────────────────────
// Each stack entry carries the [tMin, tMax] ray interval for that node.
// The near child is always processed first (pushed last onto the stack).
// Early exit: if the front of the stack has tMin > current best hit, we stop.
struct KDStackEntry {
    uint  nodeIdx;
    float tMin;
    float tMax;
};

HitInfo traceScene(inout Ray ray) {
    HitInfo hit;
    hit.hit = false;
    hit.t   = 1e10;

    // Intersect root AABB to get the initial ray interval.
    KDNode root = kdNodes[0];
    float  rootTMin, rootTMax;
    if (!intersectAABBInterval(ray, root.data0.xyz, root.data1.xyz, rootTMin, rootTMax))
        return hit;

    KDStackEntry stack[64];
    int top    = 0;
    stack[0]   = { 0u, rootTMin, rootTMax };

    while (top >= 0) {
        KDStackEntry e = stack[top--];

        // If the closest point in this node is already farther than the best
        // hit we've found, everything remaining on the stack is also farther.
        if (e.tMin > hit.t)
            continue;

        KDNode node  = kdNodes[e.nodeIdx];
        uint   flags = asuint(node.data1.w);
        bool   isLeaf = (flags >> 31) != 0u;

        if (isLeaf) {
            uint triCount  = flags & 0x1FFFFFFFu;
            uint firstPrim = asuint(node.data0.w);
            for (uint i = 0u; i < triCount; i++) {
                uint triIdx = kdTriIndices[firstPrim + i];
                float t; float2 bary;
                if (intersectTriangle(ray, triangles[triIdx], t, bary) && t < hit.t)
                    resolveHit(triIdx, t, bary, ray, hit);
            }
        } else {
            uint  leftChild = flags & 0x1FFFFFFFu;
            uint  axis      = (flags >> 29) & 3u;
            float splitPos  = node.data0.w; // stored as plain float for interior nodes

            // Parametric t of the ray at the split plane.
            float tSplit = (splitPos - ray.origin[axis]) / ray.direction[axis];

            // Near child = the side the ray origin is on.
            bool  originLeft = ray.origin[axis] < splitPos;
            uint  nearChild  = originLeft ? leftChild : leftChild + 1u;
            uint  farChild   = originLeft ? leftChild + 1u : leftChild;

            if (top < 62) {
                if (tSplit >= e.tMax) {
                    // Ray exits before reaching the split: only visit near side.
                    stack[++top] = { nearChild, e.tMin, e.tMax };
                } else if (tSplit <= e.tMin) {
                    // Ray enters after the split: only visit far side.
                    stack[++top] = { farChild, e.tMin, e.tMax };
                } else {
                    // Ray straddles the split: visit near first, far second.
                    // Push far first so near is on top of the stack.
                    stack[++top] = { farChild,  tSplit, e.tMax };
                    stack[++top] = { nearChild, e.tMin, tSplit };
                }
            }
        }
    }

    return hit;
}

bool metall_scatter(Ray r_in, HitInfo hit, Material mat, inout RNG rng,
                    inout float3 attenuation, inout Ray scattered) {
    float3 r = reflect(r_in.direction, hit.normal);
    r = normalize(r) + (FUZZ * random_on_hemisphere(hit.normal, rng));
    scattered.origin    = hit.position + hit.normal * 1e-4;
    scattered.direction = r;
    attenuation = getBaseColor(mat, hit).xyz;
    return true;
}

bool lambertian_scatter(Ray r_in, HitInfo hit, Material mat, inout RNG rng,
                        inout float3 attenuation, inout Ray scattered) {
    float3 d = normalize(hit.normal + random_on_hemisphere(hit.normal, rng));
    if (near_zero(d)) d = hit.normal;
    scattered.origin    = hit.position + hit.normal * 1e-4;
    scattered.direction = d;
    attenuation = getBaseColor(mat, hit).xyz;
    return true;
}

bool scatter(Ray r_in, HitInfo hit, inout RNG rng,
             inout float3 attenuation, inout Ray scattered, out bool isSpecular) {
    Material mat      = materials[hit.materialIndex];
    float4   mrFactor = getMetallicRoughness(mat, hit);
    if (mrFactor.b > 0.5) {
        isSpecular = true;
        return metall_scatter(r_in, hit, mat, rng, attenuation, scattered);
    } else {
        isSpecular = false;
        return lambertian_scatter(r_in, hit, mat, rng, attenuation, scattered);
    }
}

// ── KD-Tree shadow ray (ordered, any-hit) ─────────────────────────────────────
bool isOccluded(float3 origin, float3 dir, float maxDist, uint skipTriIdx) {
    Ray shadowRay;
    shadowRay.origin    = origin;
    shadowRay.direction = dir;

    KDNode root = kdNodes[0];
    float  rootTMin, rootTMax;
    if (!intersectAABBInterval(shadowRay, root.data0.xyz, root.data1.xyz, rootTMin, rootTMax))
        return false;

    KDStackEntry stack[64];
    int top  = 0;
    stack[0] = { 0u, rootTMin, min(rootTMax, maxDist) };

    while (top >= 0) {
        KDStackEntry e = stack[top--];
        if (e.tMin >= maxDist) continue;

        KDNode node  = kdNodes[e.nodeIdx];
        uint   flags = asuint(node.data1.w);
        bool   isLeaf = (flags >> 31) != 0u;

        if (isLeaf) {
            uint triCount  = flags & 0x1FFFFFFFu;
            uint firstPrim = asuint(node.data0.w);
            for (uint i = 0u; i < triCount; i++) {
                uint triIdx = kdTriIndices[firstPrim + i];
                if (triIdx == skipTriIdx) continue;
                float t; float2 bc;
                if (intersectTriangle(shadowRay, triangles[triIdx], t, bc) && t < maxDist - 1e-4)
                    return true;
            }
        } else {
            uint  leftChild = flags & 0x1FFFFFFFu;
            uint  axis      = (flags >> 29) & 3u;
            float splitPos  = node.data0.w;
            float tSplit    = (splitPos - shadowRay.origin[axis]) / shadowRay.direction[axis];

            bool originLeft = shadowRay.origin[axis] < splitPos;
            uint nearChild  = originLeft ? leftChild : leftChild + 1u;
            uint farChild   = originLeft ? leftChild + 1u : leftChild;

            if (top < 62) {
                if (tSplit >= e.tMax)
                    stack[++top] = { nearChild, e.tMin, e.tMax };
                else if (tSplit <= e.tMin)
                    stack[++top] = { farChild, e.tMin, e.tMax };
                else {
                    stack[++top] = { farChild,  tSplit, e.tMax };
                    stack[++top] = { nearChild, e.tMin, tSplit };
                }
            }
        }
    }
    return false;
}

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

float3 estimateDirect(HitInfo hit, Material mat, inout RNG rng) {
    uint   lightIdx = min(uint(rng_next(rng) * float(scene.emissiveLightCount)),
                          scene.emissiveLightCount - 1);
    float3 lightNormal;
    float  area;
    float3 lightPos   = sampleEmissiveTriangle(lightIdx, rng, lightNormal, area);
    float3 toLight    = lightPos - hit.position;
    float  dist       = length(toLight);
    float3 toLightDir = toLight / dist;

    float cosTheta      = dot(hit.normal,  toLightDir);
    float cosThetaLight = abs(dot(lightNormal, toLightDir));

    if (cosTheta <= 0.0) return float3(0.0, 0.0, 0.0);

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
                pixelColor += throughput * float3(0.70, 0.80, 1.00);
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
    RNG  rng;
    rng_init(rng, pixelCoord, pushConsts.sampleIndex);
    Ray    ray   = generateEyeRay(pixelCoord, dimensions, rng);
    float4 color = ray_color(ray, rng);

    if (pushConsts.sampleIndex == 0)
        accumBuffer[pixelIndex] = color;
    else
        accumBuffer[pixelIndex] += color;

    if (pushConsts.sampleIndex == scene.samplesPerPixel - 1) {
        float4 final = accumBuffer[pixelIndex] / float(scene.samplesPerPixel);
        final.x = linear_to_gamma(final.x);
        final.y = linear_to_gamma(final.y);
        final.z = linear_to_gamma(final.z);
        outputImage[pixelCoord] = final;
    }
}
