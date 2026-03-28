# Vulkan Experimental

![Vulkan_PBRNetural](https://github.com/user-attachments/assets/d610729c-0a8e-42a4-b174-4a836f001695)

Vulkan Experimental is a rendering project intended to teach myself not just how Vulkan works compared to DirectX, but also how a game engine is actually structured. This project implements several features found in modern graphics renderers and game engines. The project remains open-source under MIT. Despite this, this is not an open project and is intended for learning purposes.

## Features

- GLTF and KTX2 support

  Dragging and dropping a GLTF or GLB while the application is running will restart it to load a different scene, supports all base GLTF functions, but only supports KTX2 extensions.

- Full PBR rendering

  Vulkan experimental implements Epic's split sum approximation as described by LearnOpenGL.

- Shadowcasting

  Objects will cast shadows on the scene and onto themselves.

- IMGUI debugging GUI, automatically populated by the CVAR class.
