target("smart_ref")
    set_kind("headeronly")
    add_headerfiles("includes/*.hpp")
    add_headerfiles("includes/smart_ref/*.hpp", {prefixdir = "smart_ref"})