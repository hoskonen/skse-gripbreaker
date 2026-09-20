-- include CommonLibSSE-NG
includes("lib/commonlibsse-ng")

-- project
set_project("GripBreaker")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- plugin target
target("GripBreaker")
    add_rules("commonlibsse-ng.plugin", {
        name = "GripBreaker",
        author = "hoskonen",
        description = "Native SKSE weapon grip and disarm mechanics"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")