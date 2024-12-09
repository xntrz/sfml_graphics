dir_bin = "%{wks.location}/bin/%{cfg.buildcfg}"
dir_obj = "%{wks.location}/obj/%{cfg.buildcfg}/%{prj.name}"
dir_src = "%{wks.location}/../../src"
dir_inc = "%{wks.location}/../../include"
dir_lib = "%{wks.location}/../../vendor"
dir_pch = "src"


workspace "SFML_GRAPHICS"
   location "build/%{_ACTION}"
   filename "SFML_GRAPHICS"
   platforms { "x64" }
   configurations { "Debug", "Release" }
   targetdir "%{dir_bin}"
   objdir "%{dir_obj}"
   compileas "C++"
   language "C++"
   rtti "Off"
   characterset "MBCS"
   filter "configurations:Debug"
      defines { "_DEBUG", }
      symbols "On"
      optimize "Off"
      intrinsics "Off"
      inlining "Disabled"
      staticruntime "On"
      runtime "Debug"
      editandcontinue "Off"
      flags { "NoIncrementalLink" }
   filter "configurations:Release"
      defines { "NDEBUG" }
      symbols "Off"
      optimize "Full"
      intrinsics "On"
      inlining "Auto"
      staticruntime "On"
      runtime "Release"
      editandcontinue "Off"
      flags { "NoIncrementalLink", "NoBufferSecurityCheck", "NoRuntimeChecks", "MultiProcessorCompile" }


project "SFML_GRAPHICS"
   kind "StaticLib"
   includedirs {    
      "%{dir_inc}",
      "%{dir_lib}/stb/include",
      "%{dir_lib}/freetype/include", 
      "%{dir_lib}/SFML/include", 
   }
   files { 
      "%{dir_src}/**.h",
      "%{dir_src}/**.c",
      "%{dir_src}/**.hpp",
      "%{dir_src}/**.inl",
      "%{dir_src}/**.cpp",
      "%{dir_src}/**.rc",
      "%{dir_src}/**.manifest",
   }
   filter { "configurations:Debug" }
      targetname "%{wks.name}d"
      architecture "x86_64"
   filter { "configurations:Release" }
      targetname "%{wks.name}"
      architecture "x86_64"
