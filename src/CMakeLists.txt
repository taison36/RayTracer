set(Raytracer_SRC
    main.cpp

    utils/utils.cpp
    render/Renderer.cpp
    render/Ray.cpp
    render/BruteForceTraceIntegrator.cpp
    render/DefaultRayCaster.cpp

    gpu/RendererGPU.cpp

    Camera.cpp

    FrameBuffer.cpp

    SceneLoader.cpp
    Scene.cpp

)

PREPEND(Raytracer_SRC)

set(Raytracer_SRC ${Raytracer_SRC} PARENT_SCOPE)
