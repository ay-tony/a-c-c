set_project("cscc-compiler")
set_version("0.0.1", { build = "%Y%m%d%H%M" })
set_xmakever("2.8.5")
set_toolchains("clang")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate")

set_languages("cxxlatest")
set_warnings("all")
set_runtimes("c++_shared")

add_requires("argparse")
add_requires("spdlog")
add_requires("antlr4-runtime")

includes("src", "test", "xmake")
