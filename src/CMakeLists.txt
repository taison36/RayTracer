set(Raytracer_SRC
    main.cpp

    core/FrameBuffer.cpp
    core/RayTracerApplication.cpp
    core/Scene.cpp

    objects/Camera.cpp

        render/gpu/Checker.cpp
    render/gpu/Renderer.cpp
    render/gpu/VkCore.cpp
    render/gpu/BruteForce.cpp

    utils/SceneLoader.cpp
    utils/utils.cpp
)

PREPEND(Raytracer_SRC)

set(Raytracer_SRC ${Raytracer_SRC} PARENT_SCOPE)
