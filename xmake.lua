add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_languages("cxx23")

add_requires("pybind11", {system = false})
add_requires("gtest", {system = false})


target("smart_ref")
    set_kind("headeronly")
    add_packages("pybind11")
    add_includedirs("include", {public = true})
    add_headerfiles("include/*.hpp")
    add_headerfiles("include/smart_ref/*.hpp", {prefixdir = "smart_ref"})

target("test_smart_ref")
    set_default(false)
    set_kind("binary")
    add_packages("pybind11", "gtest")
    add_deps("smart_ref")
    add_files("tests/*.cpp")

    set_targetdir(".")
