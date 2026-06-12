set(Raytracer_SRC
    main.cpp

    core/FrameBuffer.cpp
    core/RayTracerApplication.cpp
    core/Scene.cpp

    objects/Camera.cpp

        render/gpu/Renderer.cpp
    render/gpu/VkCore.cpp
        render/gpu/accelerationStructures/BruteForce.cpp
        render/gpu/accelerationStructures/bvh/BVH.cpp
        render/gpu/accelerationStructures/bvh/builder/MiddlePointBuilder.cpp
        render/gpu/accelerationStructures/bvh/builder/SAHBuilder.cpp
        render/gpu/accelerationStructures/kdtree/KDTree.cpp
        render/gpu/accelerationStructures/kdtree/builder/BinnedSHABuilder.cpp
        render/gpu/accelerationStructures/kdtree/builder/ExactSAHBuilder.cpp
        render/gpu/accelerationStructures/kdtree/builder/ExactSAHBuilderNoESC.cpp

    utils/SceneLoader.cpp
    utils/utils.cpp
)

PREPEND(Raytracer_SRC)

set(Raytracer_SRC ${Raytracer_SRC} PARENT_SCOPE)
