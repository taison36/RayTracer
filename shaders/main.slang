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


[vk::binding(0, 0)] RWTexture2D<float4>                outputImage;
[vk::binding(1, 0)] StructuredBuffer<Vertex>           vertices;
[vk::binding(2, 0)] StructuredBuffer<Triangle>         triangles;
[vk::binding(3, 0)] StructuredBuffer<Material>         materials;
[vk::binding(4, 0)] StructuredBuffer<uint>             emissiveLight;
[vk::binding(5, 0)] StructuredBuffer<DirectionalLight> directionalLight;
[vk::binding(6, 0)] StructuredBuffer<PointLight>       pointLight;
[vk::binding(7, 0)] StructuredBuffer<SpotLight>        spotLight;
[vk::binding(8, 0)] ConstantBuffer<SceneSettings>      scene;
[vk::binding(9, 0)] Sampler2D                          textures[];

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

// util functions
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
        float3 p = rng_next3(rng) * 2.0 - 1.0; // [0,1] -> [-1,1]
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

HitInfo traceScene(inout Ray ray) {
    HitInfo hit;
    hit.hit = false;
    hit.t = 1e10;
    
    for (uint i = 0; i < scene.triangleCount; i++) {
        float t;
        float2 barycentrics;
        
        if (intersectTriangle(ray, triangles[i], t, barycentrics) && t < hit.t) {
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
            
            set_face_normal(ray, hit);

            hit.hit = true;
            hit.t = t;
            hit.position = ray.origin + ray.direction * t;
            hit.materialIndex = tri.material;
            Material mat = materials[hit.materialIndex];
            
            hit.texCoord = 
                v0.texCoord[mat.pbr.baseColorTexture.texCoordSet].xy * w +
                v1.texCoord[mat.pbr.baseColorTexture.texCoordSet].xy * barycentrics.x +
                v2.texCoord[mat.pbr.baseColorTexture.texCoordSet].xy * barycentrics.y;

            if (mat.pbr.metallicRoughnessTexture.index >= 0) {
                hit.mrCoord= 
                    v0.texCoord[mat.pbr.metallicRoughnessTexture.texCoordSet].xy * w +
                    v1.texCoord[mat.pbr.metallicRoughnessTexture.texCoordSet].xy * barycentrics.x +
                    v2.texCoord[mat.pbr.metallicRoughnessTexture.texCoordSet].xy * barycentrics.y;
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

bool scatter(Ray r_in, HitInfo hit, RNG rng, inout float3 attenuation, inout Ray scattered) {
    Material mat = materials[hit.materialIndex];
    float4 mrFactor = getMetallicRoughness(mat, hit);
    if (mrFactor.b > 0.5) {
        return metall_scatter(r_in, hit, mat, rng, attenuation, scattered);
    } else {
        return lambertian_scatter(r_in, hit, mat, rng, attenuation, scattered);
    }
}

float4 ray_color(Ray r, inout RNG rng) {
    float3 throughput = float3(1.0, 1.0, 1.0); // накопление энергии
    float3 pixelColor = float3(0.0, 0.0, 0.0);

    for (int bounce = 0; bounce < scene.maxBounces; ++bounce) {
        HitInfo hit = traceScene(r);
        if (hit.hit) {
            // Add emissive contribution
            //Material mat = materials[hit.materialIndex];
            //float3 emitted = mat.emissiveFactor.rgb;
            //pixelColor += throughput * emitted;

            Ray scattered;
            float3 attenuation;
            if (scatter(r, hit, rng, attenuation, scattered)) {
                r = scattered;
                throughput *= attenuation;
            } else {
                break;
            }
        }
        else {
            // Ray missed or hit background triangle - return sky color
            float3 unitDir = normalize(r.direction);
            float a = 0.5 * (unitDir.y + 1.0);
            float3 sky = (1.0 - a) * float3(1.0, 1.0, 1.0) 
                       + a * float3(0.5, 0.7, 1.0);
            
            pixelColor += throughput * sky;
            break;
        }
    }

    return float4(pixelColor, 1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 pixelCoord = dispatchThreadID.xy;
    uint2 dimensions;
    outputImage.GetDimensions(dimensions.x, dimensions.y);
    
    if (pixelCoord.x >= dimensions.x || pixelCoord.y >= dimensions.y)
        return;

    float4 color = float4(0, 0, 0, 0);
    for (int i = 0; i < scene.samplesPerPixel; ++i) {
        RNG rng;
        rng_init(rng, pixelCoord, i); 
        Ray ray = generateEyeRay(pixelCoord, dimensions, rng);
        color += ray_color(ray, rng);
    }
    
    color = color / scene.samplesPerPixel;
    color.x = linear_to_gamma(color.x);
    color.y = linear_to_gamma(color.y);
    color.z = linear_to_gamma(color.z);
    
    outputImage[pixelCoord] = color;
}
