---@diagnostic disable: undefined-global, undefined-field

workspace("gitfluss")
    configurations({"debug", "asan", "release"})
    platforms({"windows", "linux"})
    location("build")
    architecture("x86_64")

project("gitfluss")
    language("C")
    cdialect("C99")
    targetname("gitfluss")
    libdirs({"./vendor/libgit2/build/",
             "./vendor/libgit2/build/Release/",
             "./vendor/libgit2/build/Debug/"})
    includedirs({"./include/",
                 "/usr/include/",
                 "./vendor/libgit2/include/",
                 "./vendor/puddle/include/"})
    debugdir("./")
    kind("ConsoleApp")
    toolset("clang")
    buildoptions({"-Wextra", "-Wall", "-Wpedantic", "-Wconversion", "-Wshadow",
                  "-Wsign-compare", "-Wtype-limits", "-Wunused"})
    links{"git2"}

    filter("configurations:asan")
        defines{"ASAN"}

    filter("configurations:debug")
        defines{"DEBUG"}

    filter("configurations:debug or asan")
        runtime("debug")
        symbols("On")
        optimize("Off")
        buildoptions({"-gfull", "-O1"})
        linkoptions({"-gfull", "-O1"})

    filter("configurations:release")
        defines{"NDEBUG"}
        staticruntime("off")
        runtime("release")
        symbols("Off")
        optimize("Speed")

    filter("platforms:linux")
        system("linux")
        defines("BUILD_LINUX")
        targetdir("bin/%{cfg.buildcfg}")
        objdir("obj")
        files({"./src/linux_gitfluss*",
               "./include/linux_gitfluss*",
               "./src/gitfluss_*",
               "./include/gitfluss_*",
               "./vendor/puddle/src/linux*",
               "./vendor/puddle/src/string_view.c"})
        linkoptions({"-lgit2", "-fuse-ld=mold"})

    filter("platforms:windows")
        system("windows")
        defines("BUILD_WINDOWS")
        targetdir("bin/%{cfg.buildcfg}")
        objdir("obj/%{cfg.buildcfg}")
        files({"./src/win32_gitfluss*",
               "./include/win32_gitfluss*",
               "./src/gitfluss_*",
               "./include/gitfluss_*",
               "./vendor/puddle/src/win32*",
               "./vendor/puddle/src/string_view.c"})
        linkoptions({"-lgit2"})

    filter({"platforms:linux", "configurations:asan"})
        buildoptions({"-fsanitize=address,leak,undefined", "-fno-omit-frame-pointer",
                      "-static-libasan"})
        linkoptions({"-fsanitize=address,leak,undefined", "-fno-omit-frame-pointer",
                     "-static-libasan"})

    filter({"platforms:windows", "configurations:debug or asan"})
        buildoptions("-gcodeview");
        linkoptions("-gcodeview");

    filter({"platforms:windows", "configurations:asan"})
        toolset("clang-cl")
        buildoptions({"/fsanitize=address", "/Zi", "/INCREMENTAL:NO"})
        linkoptions{"/link ../vendor/libgit2/build/Debug/git2.lib clang_rt.asan_dynamic-x86_64.lib clang_rt.asan_dynamic_runtime_thunk-x86_64.lib"}
        editandcontinue("Off")
