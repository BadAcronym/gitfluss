---@diagnostic disable: undefined-global, undefined-field

workspace("gitfluss")
    configurations({"debug", "asan", "release"})
    platforms({"linux", "windows"})
    location("build")
    architecture("x86_64")

project("gitfluss")
    language("C")
    cdialect("C99")
    warnings("Extra")
    targetname("gitfluss")
    libdirs({"./vendor/river2D/bin/%{cfg.buildcfg}/",
             "./vendor/river2D/vendor/imgsurf/bin/%{cfg.buildcfg}/"})
    includedirs({"./include/",
                 "/usr/include/",
                 "./vendor/puddle/include/",
                 "./vendor/river2D/include/",
                 "./vendor/river2D/vendor/imgsurf/include"})
    debugdir("./")
    kind("ConsoleApp")

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

    filter("platforms:Linux")
        system("Linux")
        defines("BUILD_LINUX")
        targetdir("bin/%{cfg.buildcfg}")
        objdir("obj/%{cfg.buildcfg}")
        files({"./src/linux_gitfluss*",
               "./include/linux_gitfluss*",
               "./src/gitfluss_*",
               "./include/gitfluss_*"})
        linkoptions({"-lgit2", "-fuse-ld=mold"})
        links("git2:static")
        buildoptions({"-Wextra", "-Wall", "-Wpedantic", "-Wconversion", "-Wshadow",
                      "-Wsign-compare"})
        toolset("clang")

    filter("platforms:Windows")
        system("Windows")
        defines("BUILD_WINDOWS")
        targetdir("bin/%{cfg.buildcfg}")
        objdir("obj/")
        files({"./src/win32_gitfluss*",
               "./include/win32_gitfluss*",
               "./src/gitfluss_*",
               "./include/gitfluss_*"})
        buildoptions{"/wd4068", "/wd4100"}
        links("git2.lib")

    filter({"platforms:Linux", "configurations:debug or asan"})
        buildoptions({"-gfull", "-O1"})
        linkoptions({"-gfull", "-O1"})

    filter({"platforms:Linux", "configurations:asan"})
        buildoptions({"-fsanitize=address,leak,undefined", "-fno-omit-frame-pointer",
                      "-static-libasan"})
        linkoptions({"-fsanitize=address,leak,undefined", "-fno-omit-frame-pointer",
                     "-static-libasan"})

    filter({"platforms:Windows", "configurations:debug or asan"})

    filter({"platforms:Windows", "configurations:asan"})
        editandcontinue("Off")
        buildoptions({"/fsanitize=address", "/Zi", "/INCREMENTAL:NO"})

    filter({"platforms:Windows", "configurations:release"})
        linkoptions("/NODEFAULTLIB:MSVCRTD")
