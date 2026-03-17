set(Raytracer_SRC
    main.cpp

    object/Model.cpp
    object/Sphere.cpp

    Camera.cpp

    Framebuffer.cpp
    LoaderGltf.cpp

    Renderer.cpp
    Ray.cpp
)

PREPEND(Raytracer_SRC)

set(Raytracer_SRC ${Raytracer_SRC} PARENT_SCOPE)
