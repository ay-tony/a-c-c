target("executable", function()
    add_deps("antlr4")
    add_deps("error_handler")
    add_deps("argument_parser")
    add_deps("spdlog")
    add_deps("visitor")

    add_files("main.cpp")
end)

target("antlr4", function()
    set_kind("static")
    set_default(false)
    add_deps("antlr4_runtime")
    add_rules("antlr4")
    set_policy("build.fence", true)

    add_files("sysy.g4")
    add_files("antlr4.cppm", { public = true })
end)

target("argument_parser", function()
    set_kind("static")
    set_default(false)
    add_deps("argparse")
    add_deps("error_handler")
    add_deps("metadata")

    add_files("argument_parser.cppm", { public = true })
end)

target("error_handler", function()
    set_kind("static")
    set_default(false)
    add_deps("metadata")
    add_deps("antlr4_runtime")

    add_files("error_handler.cppm", { public = true })
end)

target("metadata", function()
    set_kind("static")
    set_default(false)
    add_includedirs("$(buildir)", { public = true })
    set_configdir("$(buildir)")

    set_configvar("PROGRAM_NAME", "acc")
    add_configfiles("metadata.h.in", { public = true })

    add_files("metadata.cppm", { public = true })
end)

target("visitor", function()
    set_kind("static")
    set_default(false)
    add_deps("antlr4")
    add_deps("symbol")
    add_deps("error_handler")

    add_files("visitor.cppm", { public = true })
    add_files("visitor.cpp")
end)

target("symbol", function()
    set_kind("static")
    set_default(false)
    add_deps("error_handler")

    add_files("symbol.cppm", { public = true })
    add_files("symbol.cpp")
end)

-- 封装外部库为 modules

target("argparse", function()
    set_kind("static")
    set_default(false)
    add_packages("argparse", { public = true })

    add_files("argparse.cppm", { public = true })
end)

target("spdlog", function()
    set_kind("static")
    set_default(false)
    add_packages("spdlog", { public = true })

    add_files("spdlog.cppm", { public = true })
end)

target("antlr4_runtime", function()
    set_kind("static")
    set_default(false)
    add_packages("antlr4-runtime", { public = true })

    add_files("antlr4_runtime.cppm", { public = true })
end)
