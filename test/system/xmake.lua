for _, file in ipairs(os.files("testcases/*/*.c")) do
    local names = {}
    for name in string.gmatch(file, "([^/]+)") do
        names[#names + 1] = name
    end
    local target_name = "system_test." .. names[#names - 1] .. "." .. path.basename(file)
    target(target_name, function()
        set_kind("phony")
        set_default(false)
        add_deps("executable")
        add_tests("default", { runargs = path.absolute(file) })

        on_test(function(target, opt)
            -- 获取系统和项目中的编译工具
            import("lib.detect.find_tool")
            local acc = path.absolute(path.join(target:targetdir(), "executable"))
            local clang = find_tool("clang").program

            -- 在 $(buildir)/.test 下面编译测试点
            import("core.project.config")
            local filename = path.filename(opt.runargs)
            local testdir = path.join(config.buildir(), ".test")
            os.mkdir(testdir)

            local acc_mid = path.absolute(path.join(testdir, filename .. ".ll"))
            local acc_out = path.absolute(path.join(testdir, filename .. "-acc"))
            local clang_out = path.absolute(path.join(testdir, filename .. "-clang"))

            -- 测试能否通过编译
            local acc_failed = false
            local clang_failed = false
            try {
                function()
                    os.runv(acc, { opt.runargs, "-o", acc_mid })
                    os.runv(clang, { acc_mid, "-o", acc_out })
                end,
                catch { function() acc_failed = true end }
            }
            try {
                function()
                    os.runv(clang, { opt.runargs, "-o", clang_out })
                end,
                catch { function() clang_failed = true end }
            }

            if (acc_failed and clang_failed) then
                return true
            elseif (acc_failed ~= clang_failed) then
                return false
            end

            -- 测试编译后的程序能否正常运行
            local outf = os.tmpfile()
            local errf = os.tmpfile()

            local acc_return
            try {
                function()
                    acc_return, _ = os.execv(acc_out, {}, { stdout = outf, stderr = errf })
                end
            }
            local acc_stdout = io.readfile(outf)
            local acc_stderr = io.readfile(errf)

            local clang_return
            try {
                function()
                    clang_return, _ = os.execv(clang_out, {}, { stdout = outf, stderr = errf })
                end
            }
            local clang_stdout = io.readfile(outf)
            local clang_stderr = io.readfile(errf)

            os.rm(outf)
            os.rm(errf)

            return (acc_stdout == clang_stdout) and
                (acc_stderr == clang_stderr) and
                (acc_return == clang_return)
        end)

        on_clean(function(target)
            import("core.project.config")
            local testdir = path.join(config.buildir(), ".test")
            os.rm(path.join(testdir, "**"))
        end)
    end)
end
