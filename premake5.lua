workspace "LinxcTest"
    configurations { "Debug", "Release" }
    architecture "x64"

project "Test"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    targetdir "linxc-out/bin/%{cfg.buildcfg}"
    includedirs {"include", "linxc-out"}
    location "linxc-out"

    files { "linxc-out/**.h", "linxc-out/**.cpp" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"