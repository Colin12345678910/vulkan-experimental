# Vulkan Experimental

![PBR](https://colin12345678910.github.io/portfolio/assets/images/Vulkan_PBRNeutral.jpg)

Vulkan Experimental is a render project intended to teach myself not just how Vulkan works in comparison to DirectX, but also how a game engine is actually structured. This project implements several features found in modern graphics renderers and game engines. The project remains open-source under MIT. Despite this, this is not an open project and is intended for learning purposes.

## Features

- GLTF and KTX2 support

  Dragging and dropping a GLTF or GLB while the application is running will restart it to load a different scene, supports all base GLTF functions, but only supports KTX2 extensions.

- Full PBR rendering

  Vulkan experimental implements Epic's split sum approximation as described by LearnOpenGL.

- Shadowcasting

  Objects will cast shadows on the scene and onto themselves.

- IMGUI debugging GUI, automatically populated by the CVAR class.
