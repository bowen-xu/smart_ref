add_requires("pybind11", {system = false})


target("smart_ref")
    set_kind("static")
    add_packages("pybind11")
    add_files("include/**.mxx", {public = true})
    -- add_includedirs("include", {public = true})
    -- add_headerfiles("include/*.hpp")
    -- add_headerfiles("include/smart_ref/*.hpp", {prefixdir = "smart_ref"})