# DirectX12 labs

This repo contains a template for Basics of DirectX 12 labs

## Pre requirements

- [Premake5](https://premake.github.io/download.html#v5)
- [Visual studio 2019 Community](https://visualstudio.microsoft.com/ru/vs/community/) or similar
- [Windows 10 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk/)
- (Optional) [RenderDoc](https://renderdoc.org/)

Don't forget `git submodule update --init --recursive` after the first clone

## How to prepare Visual Studio solution

Go to the project folder and run:

```sh
premake5 vs2019
```

## How to check DX12 installation

1. Prepare the solution
2. Build **DX12 installation check** project
3. Run the project and check list of your GPUs

## Third-party tools and data

- [tinyobjloader](https://github.com/syoyo/tinyobjloader) by Syoyo Fujita (MIT License)
- [Cornell Box models](https://casual-effects.com/g3d/data10/index.html#) by Morgan McGuire (CC BY 3.0 License)
- [D3D12 Helper Library](https://github.com/Microsoft/DirectX-Graphics-Samples/tree/master/Libraries/D3DX12)