# ExamMode plugin - Veyon IFPR (REA)
# Copyright (c) 2026 IFPR - Campus União da Vitória - GNU GPL v2 or later

include(BuildVeyonPlugin)

set(internetcontrol_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../internetcontrol)

build_veyon_plugin(exammode
	NAME ExamMode
	SOURCES
	ExamModeFeaturePlugin.cpp
	ExamModeFeaturePlugin.h
	ExamModeOverlay.cpp
	ExamModeOverlay.h
	UsbStorageControl.cpp
	UsbStorageControl.h
	${internetcontrol_DIR}/InternetAccessBlocker.cpp
	${internetcontrol_DIR}/InternetAccessBlocker.h
	exammode.qrc
	)

target_include_directories(exammode PRIVATE ${internetcontrol_DIR})

# Windows API sources (INITGUID, windows.h macros) must not share a PCH/unity unit with Qt code
set_source_files_properties(${internetcontrol_DIR}/InternetAccessBlocker.cpp UsbStorageControl.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE SKIP_PRECOMPILE_HEADERS TRUE)

if(VEYON_BUILD_WINDOWS)
	target_link_libraries(exammode PRIVATE -lole32 -loleaut32 -luuid -ladvapi32)
endif()
