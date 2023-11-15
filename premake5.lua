workspace "LinxcTest"
    configurations { "Debug", "Release" }
    architecture "x64"

project "Linxcc"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/linxcc/%{cfg.buildcfg}"
    includedirs {"src/include", "src/linxcstd"}
    location "src"

    files { "src/**.h", "src/**.c", "src/**.hpp", "src/**.cpp", "src/**.linxc" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

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