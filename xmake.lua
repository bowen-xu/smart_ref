add_requires("pybind11", {system = false})


target("smart_ref")
    set_kind("headeronly")
    add_packages("pybind11")
    add_includedirs("include", {public = true})
    add_headerfiles("include/*.hpp")
    add_headerfiles("include/smart_ref/*.hpp", {prefixdir = "smart_ref"})