# InternetControl plugin - Veyon IFPR (REA)
# Copyright (c) 2026 IFPR - Campus União da Vitória - GNU GPL v2 or later

include(BuildVeyonPlugin)

build_veyon_plugin(internetcontrol
	NAME InternetControl
	SOURCES
	InternetControlFeaturePlugin.cpp
	InternetControlFeaturePlugin.h
	InternetAccessBlocker.cpp
	InternetAccessBlocker.h
	internetcontrol.qrc
	)

# InternetAccessBlocker.cpp defines INITGUID and therefore must not share a PCH/unity unit
set_source_files_properties(InternetAccessBlocker.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE SKIP_PRECOMPILE_HEADERS TRUE)

if(VEYON_BUILD_WINDOWS)
	target_link_libraries(internetcontrol PRIVATE -lole32 -loleaut32 -luuid)
endif()
