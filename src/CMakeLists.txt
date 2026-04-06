set(Raytracer_SRC
    main.cpp

    core/FrameBuffer.cpp
    core/RayTracerApplication.cpp
    core/Scene.cpp

    objects/Camera.cpp

    render/cpu/Ray.cpp
    render/cpu/BruteForceTraceIntegrator.cpp
    render/cpu/DefaultRayCaster.cpp
    render/cpu/Renderer.cpp

    render/gpu/BruteForce.cpp
    render/gpu/Renderer.cpp
    render/gpu/VkCore.cpp

    utils/SceneLoader.cpp
    utils/utils.cpp
)

PREPEND(Raytracer_SRC)

set(Raytracer_SRC ${Raytracer_SRC} PARENT_SCOPE)
