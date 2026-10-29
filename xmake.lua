-- enable CommonLibSSE-NG's built-in INI support
set_config("rex_ini", true)

-- include CommonLibSSE-NG
includes("lib/commonlibsse-ng")

-- project
set_project("GripBreaker")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- plugin target
target("GripBreaker")
    add_rules("commonlibsse-ng.plugin", {
        name = "GripBreaker",
        author = "hoskonen",
        description = "Native SKSE weapon grip and disarm mechanics"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "lib/skse-menu-framework/include")
    set_pcxxheader("src/pch.h")

-- dependency-free tests for the pure Phase 1 grip formula
target("GripEvaluationTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/GripEvaluationTests.cpp", "src/GripEvaluation.cpp", "src/DisarmEvaluation.cpp")
    add_includedirs("src")

-- dependency-free tests for logical-clash weapon mass evaluation
target("MassMatchupEvaluationTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/MassMatchupEvaluationTests.cpp", "src/MassMatchupEvaluation.cpp")
    add_includedirs("src")

-- dependency-free tests for logical-clash grip configuration evaluation
target("GripConfigurationEvaluationTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/GripConfigurationEvaluationTests.cpp", "src/GripConfigurationEvaluation.cpp")
    add_includedirs("src")

-- dependency-free tests for logical-clash stamina grip evaluation
target("StaminaGripEvaluationTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/StaminaGripEvaluationTests.cpp", "src/StaminaGripEvaluation.cpp")
    add_includedirs("src")

-- dependency-free tests for logical-clash weapon skill grip evaluation
target("SkillGripEvaluationTests")
    set_kind("binary")
    set_default(false)
    add_files(
        "tests/SkillGripEvaluationTests.cpp",
        "src/GripConfigurationEvaluation.cpp",
        "src/SkillGripEvaluation.cpp",
        "src/StaminaGripEvaluation.cpp"
    )
    add_includedirs("src")

-- dependency-free tests for directional logical-clash severity evaluation
target("ClashSeverityEvaluationTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/ClashSeverityEvaluationTests.cpp", "src/ClashSeverityEvaluation.cpp")
    add_includedirs("src")

-- dependency-free tests for canonical clash probability estimates
target("ClashProbabilityEvaluationTests")
    set_kind("binary")
    set_default(false)
    add_files(
        "tests/ClashProbabilityEvaluationTests.cpp",
        "src/ClashProbabilityEvaluation.cpp",
        "src/ClashSeverityEvaluation.cpp"
    )
    add_includedirs("src")

-- deterministic decisions and RNG invocation count, without Skyrim dependencies
target("ClashDisarmDecisionTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/ClashDisarmDecisionTests.cpp", "src/ClashDisarmDecision.cpp")
    add_includedirs("src")
