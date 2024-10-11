rule("antlr4", function()
    add_deps("c++")
    set_extensions(".g4")

    on_load(function(target)
        if target:sourcebatches()["antlr4"] then
            -- 检查并获取 antlr4 命令
            import("lib.detect.find_tool")
            local antlr4 = assert(
                find_tool("antlr4", {
                    check = function(tool)
                        os.run("%s", tool)
                    end
                }),
                "antlr4 not found!")

            -- 生成 .cpp 和 .h 文件的临时存放目录，并加入头文件搜索
            local gendir = path.absolute(path.join(target:autogendir(), "rules", "antlr4"))
            os.mkdir(gendir)
            target:add("includedirs", gendir, { public = true })

            -- 遍历所有源文件
            for _, sourcefile in ipairs(target:sourcebatches()["antlr4"]["sourcefiles"]) do
                -- 开始生成 .cpp 和 .h 文件
                os.cd(path.absolute(path.directory(sourcefile)))
                os.vrunv(antlr4.program, {
                    "-visitor",
                    "-no-listener",
                    "-Dlanguage=Cpp",
                    "-o",
                    gendir,
                    path.absolute(path.filename(sourcefile)) })
                os.cd("-")
            end

            -- 添加生成的 .cpp 文件到源文件
            for _, genfile in ipairs(os.files(path.join(gendir, "*.cpp"))) do
                target:add("files", genfile)
            end
        end
    end)
end)
