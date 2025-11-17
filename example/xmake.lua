add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_languages("cxx23")


project_root = path.join(os.scriptdir(), ".")
local py_root = project_root

add_requires("fmt")
add_requires("pybind11", {system = false})


set_toolchains("llvm")

includes = {
    path.join(os.scriptdir(), "../includes"),
}

srcs = table.join(
    "main.cpp"
)

target("main")
    set_kind("binary")
    add_packages("fmt")
    add_files(srcs)
    add_includedirs(
        includes
    )


    set_targetdir(project_root)
    set_policy("build.c++.modules", true)


srcs_py = table.join(
    "foo.cpp"
)

target("foo")
    add_defines("PYMODULE")
    add_rules("python.module")
    add_packages("pybind11")

    add_files(srcs_py)
    add_includedirs(
        includes
    )

    set_targetdir(project_root)

    after_build(function (target)
        cprint("${blue}Generate stub for " .. target:name() .. "...")
        local py = os.getenv("CONDA_PREFIX") and (os.getenv("CONDA_PREFIX") .. "/bin/python") or "python"
        cprint("${yellow}Using python: " .. py)
        os.exec(py .. " --version")
        os.exec(py .. " " .. py_root .. "/_generate_stub.py " .. " --root " .. py_root .. " -p " .. target:name() .. " --single True")
        -- the relevant modules are imported in the cpp code, so no need to `eval` to import them here
    end)