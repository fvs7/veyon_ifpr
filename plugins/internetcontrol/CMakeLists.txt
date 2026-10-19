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

if(VEYON_BUILD_LINUX)
	if(NOT SYSTEMD_SERVICE_INSTALL_DIR)
		set(SYSTEMD_SERVICE_INSTALL_DIR /lib/systemd/system)
	endif()
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/veyon-ifpr-restore.service DESTINATION ${SYSTEMD_SERVICE_INSTALL_DIR})
	# enable the unit statically via the package, so no "systemctl enable" is needed after installation
	file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/multi-user.target.wants)
	file(CREATE_LINK ../veyon-ifpr-restore.service
		${CMAKE_CURRENT_BINARY_DIR}/multi-user.target.wants/veyon-ifpr-restore.service SYMBOLIC)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/multi-user.target.wants/veyon-ifpr-restore.service
		DESTINATION ${SYSTEMD_SERVICE_INSTALL_DIR}/multi-user.target.wants)
endif()
