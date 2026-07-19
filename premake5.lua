---@diagnostic disable: undefined-global, undefined-field

workspace("gitfluss")
    configurations({"debug", "asan", "release"})

    if package.config:sub(1,1) == '\\' then
        platforms { "x64" }
    else
        platforms { "linux" }
    end

    location("build")
    architecture("x86_64")

project("gitfluss")
    language("C")
    cdialect("C99")
    warnings("Extra")
    targetname("gitfluss")
    libdirs({"./vendor/river2D/bin/%{cfg.buildcfg}/",
             "./vendor/river2D/vendor/imgsurf/bin/%{cfg.buildcfg}/",
             "./vendor/libgit2/build/Release/",
             "./vendor/libgit2/build/Debug/"})
    includedirs({"./include/",
                 "/usr/include/",
                 "./vendor/libgit2/include/",
                 "./vendor/puddle/include/",
                 "./vendor/river2D/include/",
                 "./vendor/river2D/vendor/imgsurf/include"})
    debugdir("./")
    kind("ConsoleApp")
    links{"git2"}

    filter("configurations:asan")
        defines{"ASAN"}

    filter("configurations:debug")
        defines{"DEBUG"}

    filter("configurations:debug or asan")
        runtime("debug")
        symbols("On")
        optimize("Off")

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
        buildoptions({"-Wextra", "-Wall", "-Wpedantic", "-Wconversion", "-Wshadow",
                      "-Wsign-compare"})
        toolset("clang")

    filter("platforms:x64")
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
        buildoptions{"/wd4068", "/wd4100"}
        toolset("msc")

    filter({"platforms:linux", "configurations:debug or asan"})
        buildoptions({"-gfull", "-O1"})
        linkoptions({"-gfull", "-O1"})

    filter({"platforms:linux", "configurations:asan"})
        buildoptions({"-fsanitize=address,leak,undefined", "-fno-omit-frame-pointer",
                      "-static-libasan"})
        linkoptions({"-fsanitize=address,leak,undefined", "-fno-omit-frame-pointer",
                     "-static-libasan"})

    filter({"platforms:x64", "configurations:debug or asan"})

    filter({"platforms:x64", "configurations:asan"})
        editandcontinue("Off")
        buildoptions({"/fsanitize=address", "/Zi", "/INCREMENTAL:NO"})

    filter({"platforms:x64", "configurations:release"})
        linkoptions("/NODEFAULTLIB:MSVCRTD")
