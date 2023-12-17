workspace "LinxcTest"
    configurations { "Debug", "Release" }
    platforms { "Windows", "MacOS", "Linux" }
    architecture "x64"

filter "platforms:Windows"
    defines { "WINDOWS" }
    system "windows"

filter "platforms:MacOS"
    defines { "MACOS", "POSIX" }
    system "macosx"

filter "platforms:Linux"
    defines { "LINUX", "POSIX" }
    system "linux"

project "Linxcc"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/linxcc/%{cfg.buildcfg}"
    includedirs {"src/include", "src/linxcstd", "src/libclang"}
    links {"src/libclang/libclang"}
    location "src"

    files { "src/**.h", "src/**.c", "src/**.hpp", "src/**.cpp", "src/**.linxc" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

-- project "Test"
--     kind "ConsoleApp"
--     language "C++"
--     cppdialect "C++17"
--     targetdir "linxc-out/bin/%{cfg.buildcfg}"
--     includedirs {"include", "linxc-out"}
--     location "linxc-out"

--     files { "linxc-out/**.h", "linxc-out/**.cpp" }

--     filter "configurations:Debug"
--         defines { "DEBUG" }
--         symbols "On"

--     filter "configurations:Release"
--         defines { "NDEBUG" }
--         optimize "On"