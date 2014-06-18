local action = _ACTION or ""

solution "ijxml"
	location ".build"
	configurations { "Debug", "Release" }
	platforms { "x64", "x32" }

	configuration { "windows" }
		defines { "WIN32", "_WIN32" }
		links { }

	configuration { "x32", "vs*" }

	configuration { "x64", "vs*" }
		defines { "_WIN64" }

	configuration { "vs*" }
		defines {
			"WIN32_LEAN_AND_MEAN",
			"VC_EXTRALEAN",

			"_SCL_SECURE_NO_WARNINGS",
			"_CRT_SECURE_NO_WARNINGS",
			"_CRT_SECURE_NO_DEPRECATE"
		}

	configuration "Debug"
		objdir ".build/obj_debug"
		targetdir ".build/bin_debug"
		defines { "DEBUG" }
		flags { "Symbols", "ExtraWarnings", "FatalWarnings" }

	configuration "Release"
		objdir ".build/obj_release"
		targetdir ".build/bin_release"
		defines { "NDEBUG" }
		flags { "Symbols", "Optimize", "ExtraWarnings", "FatalWarnings" }

	project "ijxml_test"
		location ".build"
		kind "ConsoleApp"
		uuid (os.uuid("ijxml_test"))

		language "C"
		files { "*.c", "*.h" }
		excludes { }
