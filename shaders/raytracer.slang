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

// -----------------------------------------------------------
// Constants
// -----------------------------------------------------------

static const float PI     = 3.14159265359f;
static const float T_MIN  = 1e-4f;
static const float T_MAX  = 1e9f;
static const uint  NO_HIT = 0xFFFFFFFFu;

// -----------------------------------------------------------
// UV helpers — resolve a texCoord set index for a vertex
// -----------------------------------------------------------

float2 interpTexCoord(Triangle tri, float3 bary, int texCoordSet)
{
    return bary.x * vertices[tri.indices[0]].texCoord[texCoordSet]
         + bary.y * vertices[tri.indices[1]].texCoord[texCoordSet]
         + bary.z * vertices[tri.indices[2]].texCoord[texCoordSet];
}

float4 sampleMaterialTex(TextureInfo textureInfo, Triangle tri, float3 bary)
{
    if (textureInfo.index < 0)
        return float4(1.0f);

    float2 uv = interpTexCoord(tri, bary, textureInfo.texCoordSet);
    return textures[textureInfo.index].SampleLevel(uv, 0.0);
}

// -----------------------------------------------------------
// RNG — PCG hash, seeded per pixel (no frame index needed)
// -----------------------------------------------------------

uint pcgHash(inout uint state)
{
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand(inout uint rng)
{
    return float(pcgHash(rng)) * (1.0f / 4294967296.0f);
}

float2 rand2(inout uint rng) { return float2(rand(rng), rand(rng)); }

// -----------------------------------------------------------
// Math helpers
// -----------------------------------------------------------

// Build orthonormal basis around N (Duff et al. 2017)
void buildONB(float3 N, out float3 T, out float3 B)
{
    float s = N.z >= 0.0f ? 1.0f : -1.0f;
    float a = -1.0f / (s + N.z);
    float b = N.x * N.y * a;
    T = float3(1.0f + s * N.x * N.x * a, s * b, -s * N.x);
    B = float3(b, s + N.y * N.y * a, -N.y);
}

float3 toWorld(float3 v, float3 N, float3 T, float3 B)
{
    return v.x * T + v.y * B + v.z * N;
}

// Cosine-weighted hemisphere sample (tangent space)
float3 cosineSampleHemisphere(float2 u)
{
    float r   = sqrt(u.x);
    float phi = 2.0f * PI * u.y;
    return float3(r * cos(phi), r * sin(phi), sqrt(max(0.0f, 1.0f - u.x)));
}


// GGX microfacet half-vector sample (tangent space)
float3 sampleGGX(float2 u, float alpha)
{
    float a2  = alpha * alpha;
    float phi = 2.0f * PI * u.x;
    float cosT = sqrt((1.0f - u.y) / max(1.0f + (a2 - 1.0f) * u.y, 1e-8f));
    float sinT = sqrt(max(0.0f, 1.0f - cosT * cosT));
    return float3(sinT * cos(phi), sinT * sin(phi), cosT);
}

// -----------------------------------------------------------
// PBR — Cook-Torrance GGX
// -----------------------------------------------------------

float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

float G_SchlickGGX(float NdotX, float roughness)
{
    float k = (roughness + 1.0f);
    k = (k * k) / 8.0f;
    return NdotX / (NdotX * (1.0f - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

// -----------------------------------------------------------
// Ray / Hit
// -----------------------------------------------------------

struct Ray
{
    float3 origin;
    float3 dir;
};

struct HitRecord
{
    float  t;
    float3 pos;
    float3 normal;    // shading normal, facing ray
    float3 bary;      // barycentric weights (w0, w1, w2) for UV interpolation
    uint   triIndex;
    bool   frontFace;
};

// -----------------------------------------------------------
// Möller–Trumbore intersection
// -----------------------------------------------------------

bool intersectTriangle(Ray ray,
                       float3 v0, float3 v1, float3 v2,
                       out float tHit, out float2 bary2) {
    tHit  = T_MAX;
    bary2 = 0;
    float3 e1  = v1 - v0;
    float3 e2  = v2 - v0;
    float3 h   = cross(ray.dir, e2);
    float  det = dot(e1, h);

    if (abs(det) < 1e-8f) return false;

    float  invDet = 1.0f / det;
    float3 s      = ray.origin - v0;
    float  u      = dot(s, h) * invDet;
    if (u < 0.0f || u > 1.0f) return false;

    float3 q = cross(s, e1);
    float  v = dot(ray.dir, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;

    tHit  = dot(e2, q) * invDet;
    bary2 = float2(u, v);
    return tHit > T_MIN;
}

// -----------------------------------------------------------
// Scene intersection — brute force over all triangles
// -----------------------------------------------------------

bool intersectScene(Ray ray, out HitRecord hit)
{
    hit.t        = T_MAX;
    hit.triIndex = NO_HIT;

    for (uint i = 0; i < scene.triangleCount; ++i) {
        Triangle tri = triangles[i];

        float3 v0 = vertices[tri.indices[0]].position;
        float3 v1 = vertices[tri.indices[1]].position;
        float3 v2 = vertices[tri.indices[2]].position;

        float  tHit;
        float2 bary2;
        if (!intersectTriangle(ray, v0, v1, v2, tHit, bary2)) continue;
        if (tHit >= hit.t) continue;

        hit.t        = tHit;
        hit.triIndex = i;

        float w1 = bary2.x;
        float w2 = bary2.y;
        float w0 = 1.0f - w1 - w2;
        hit.bary = float3(w0, w1, w2);

        // Interpolate shading normal
        float3 n0 = vertices[tri.indices[0]].normal;
        float3 n1 = vertices[tri.indices[1]].normal;
        float3 n2 = vertices[tri.indices[2]].normal;
        float3 n  = normalize(w0 * n0 + w1 * n1 + w2 * n2);

        hit.pos       = ray.origin + tHit * ray.dir;
        hit.frontFace = dot(ray.dir, n) < 0.0f;
        hit.normal    = hit.frontFace ? n : -n;
    }

    return hit.triIndex != NO_HIT;
}

// -----------------------------------------------------------
// Camera ray generation
// -----------------------------------------------------------

Ray generateRay(uint2 pixel, uint2 dims, inout uint rng)
{
    float2 jitter = rand2(rng) - 0.5f;
    float2 uv     = (float2(pixel) + 0.5f + jitter) / float2(dims);
    uv            = uv * 2.0f - 1.0f;
    uv.y          = -uv.y;   // Vulkan Y flip

    float4 target   = mul(scene.camera.projInverse, float4(uv, 1.0f, 1.0f));
    float3 dirView  = normalize(target.xyz / target.w);
    float3 dirWorld = normalize(mul((float3x3)scene.camera.viewInverse, dirView));

    Ray ray;
    ray.origin = scene.camera.position;
    ray.dir    = dirWorld;
    return ray;
}

// -----------------------------------------------------------
// Path tracer
// -----------------------------------------------------------

float3 pathTrace(Ray ray, inout uint rng) {
    float3 radiance   = float3(0.0f);
    float3 throughput = float3(1.0f);

    for (uint bounce = 0; bounce < scene.maxBounces; ++bounce) {
        HitRecord hit;
        if (!intersectScene(ray, hit)) {
            // Simple sky gradient
            float  t   = 0.5f * (normalize(ray.dir).y + 1.0f);
            float3 sky = lerp(float3(1.0f), float3(0.5f, 0.7f, 1.0f), t);
            radiance  += throughput * sky;
            break;
        }

        Triangle tri = triangles[hit.triIndex];
        Material mat = materials[tri.material];

        // Emissive
        radiance += throughput * mat.emissiveFactor;

        // Base color — use the UV set specified by the texture info
        float4 baseColor = mat.pbr.baseColorFactor
                         * sampleMaterialTex(mat.pbr.baseColorTexture, tri, hit.bary);

        // Alpha cutout
        if (baseColor.a < 0.5f) break;

        // Metallic / roughness (G channel = roughness, B channel = metallic per glTF)
        float4 mrSample  = sampleMaterialTex(mat.pbr.metallicRoughnessTexture, tri, hit.bary);
        float  metallic  = clamp(mat.pbr.metallicFactor  * mrSample.b, 0.0f, 1.0f);
        float  roughness = clamp(mat.pbr.roughnessFactor * mrSample.g, 0.04f, 1.0f);

        float3 N = hit.normal;
        float3 V = -ray.dir;
        float  NdotV = max(dot(N, V), 1e-4f);

        float3 F0 = lerp(float3(0.04f), baseColor.rgb, metallic);
        float3 T, B;
        buildONB(N, T, B);

        // Stochastic BRDF lobe selection
        float  specProb  = max(F0.r, max(F0.g, F0.b));
        bool   doSpecular = rand(rng) < specProb;

        float3 newDir;
        float3 weight;

        if (doSpecular) {
            // GGX specular
            float3 H = toWorld(sampleGGX(rand2(rng), roughness * roughness), N, T, B);
            newDir   = reflect(-V, H);

            if (dot(newDir, N) <= 0.0f) break;

            float NdotL = max(dot(N, newDir), 1e-4f);
            float NdotH = max(dot(N, H),      1e-4f);
            float VdotH = max(dot(V, H),      1e-4f);

            float3 F = F_Schlick(VdotH, F0);
            float  G = G_Smith(NdotV, NdotL, roughness);

            // Importance-sampled Cook-Torrance weight:  F*G*VdotH / (NdotH * NdotV)
            weight  = (F * G * VdotH) / max(NdotH * NdotV, 1e-4f);
            weight /= specProb;
        }
        else
        {
            // Lambertian diffuse — cosine PDF cancels with NdotL
            newDir  = toWorld(cosineSampleHemisphere(rand2(rng)), N, T, B);

            float3 F  = F_Schlick(NdotV, F0);
            float3 kD = (1.0f - F) * (1.0f - metallic);
            weight    = kD * baseColor.rgb;
            weight   /= (1.0f - specProb);
        }

        throughput *= weight;

        // Russian roulette termination (after bounce 3)
        if (bounce >= 3u)
        {
            float p = max(throughput.r, max(throughput.g, throughput.b));
            if (rand(rng) > p) break;
            throughput /= p;
        }

        ray.origin = hit.pos + hit.normal * 1e-4f;
        ray.dir    = newDir;
    }

    return radiance;
}

// -----------------------------------------------------------
// Compute entry point
// -----------------------------------------------------------

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 DTid : SV_DispatchThreadID) {
    uint2 dims;
    outputImage.GetDimensions(dims.x, dims.y);
    if (any(DTid.xy >= dims)) return;

    uint2  pixel = DTid.xy;
    uint   t = pixel.x * 1973u + pixel.y * 9277u + 1u;
    uint   rng   = pcgHash(t);

    float3 color = float3(0.0f);
    for (uint s = 0; s < scene.samplesPerPixel; ++s)
    {
        Ray ray = generateRay(pixel, dims, rng);
        color  += pathTrace(ray, rng);
    }
    color /= float(scene.samplesPerPixel);

    // Reinhard tone mapping + gamma correction
    color = color / (color + 1.0f);
    color = pow(color, 1.0f / 2.2f);

    outputImage[pixel] = float4(color, 1.0f);
}
