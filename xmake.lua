---@diagnostic disable: undefined-global

set_project("IIRA")
set_version("0.1.0")

add_rules("mode.debug", "mode.release")
set_defaultmode("debug")

add_rules("plugin.compile_commands.autoupdate")
set_languages("gnu23")

if is_mode("release") then
    set_warnings("everything")
    set_symbols("hidden")
    set_optimize("fastest")
else
    set_warnings("all", "extra")
    set_symbols("debug")
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.leak", true)
    set_policy("build.sanitizer.undefined", true)
    add_defines("DEBUG")
end

target("unfinity", function()
    set_kind("$(kind)")
    add_includedirs("unfinity/include", { public = true })
    add_files("unfinity/source/*.c")
end)

target("iirac", function()
    set_kind("binary")
    set_default(true)
    add_deps("unfinity")
    add_includedirs("iirac/include")
    add_files("iirac/source/*.c")
end)
