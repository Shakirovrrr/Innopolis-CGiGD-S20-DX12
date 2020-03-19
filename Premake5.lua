workspace "Basics of DirectX 12"
   configurations { "Debug", "Release" }
   language "C++"
   architecture "x64"
   systemversion "latest"
   toolset "v142"
   optimize "Speed"
   links { "d3d12", "dxgi", "d3dcompiler" }
   filter("configurations:Debug")
      defines({ "DEBUG" })
      symbols("On")
      optimize("Off")
      targetdir ("bin/debug")
   filter("configurations:Release")
      defines({ "NDEBUG" })
      symbols("On")
      targetdir ("bin/release")

   project "DX12 installation check"
      kind "ConsoleApp"
      entrypoint "WinMainCRTStartup"
      includedirs { "src" }
      includedirs { "libs/D3DX12" }
      files { "src/dx12_labs.h" }
      files {"src/dx12_check_main.cpp" }

   project "DX12 window"
      kind "WindowedApp"
      entrypoint "WinMainCRTStartup"
      includedirs { "src" }
      includedirs { "libs/D3DX12" }
      includedirs { "libs/tinyobjloader" }
      files { "src/dx12_labs.h" }
      files { "src/renderer.h", "src/renderer.cpp"}
      files { "src/win32_window.h", "src/win32_window.cpp"}
      files { "src/win32_window_main.cpp" }
      files { "libs/tinyobjloader/tiny_obj_loader.h"}
      postbuildcommands {
         "{COPY} shaders/shaders.hlsl %{cfg.buildtarget.directory}",
         "{COPY} models/CornellBox-Original.obj %{cfg.buildtarget.directory}",
         "{COPY} models/CornellBox-Original.mtl %{cfg.buildtarget.directory}"
       }