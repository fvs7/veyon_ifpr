/*
 * UsbStorageControl.h - declaration of UsbStorageControl class
 *
 * Copyright (c) 2026 Instituto Federal do Paraná (IFPR) - Campus União da Vitória
 *
 * This file is part of Veyon IFPR (REA) - a derivative work of Veyon (https://veyon.io)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

/*!
 * \brief Temporarily disables the USB mass storage driver (USBSTOR) on Windows
 *
 * disable() sets HKLM\SYSTEM\CurrentControlSet\Services\USBSTOR\Start = 4 (disabled)
 * after backing up the previous value to HKLM\SOFTWARE\Veyon IFPR\ExamMode\UsbStorOriginalStart.
 * restore() writes the backed up value (normally 3 = on demand) and deletes the backup.
 *
 * The backup doubles as the persistent "exam mode active" marker, so the state
 * survives reboots and service restarts.
 *
 * Note: the setting only affects devices plugged in AFTER it was applied - storage
 * devices which are already mounted remain usable until they are unplugged.
 *
 * On Linux disable() installs the udev rule LinuxUdevRuleFile, which deauthorizes every
 * newly attached USB interface of class 08 (mass storage); restore() removes it again.
 * The rule file is the persistent "exam mode active" marker. As on Windows, devices
 * which are already attached are not affected. Requires root privileges (Veyon Server
 * in an X11 session, e.g. Xubuntu).
 *
 * On other platforms all methods are no-ops which return false.
 */
class UsbStorageControl
{
public:
	static bool disable();
	static bool restore();
	static bool isDisabledByExamMode();

	static constexpr unsigned long StartDisabled = 4;
	static constexpr unsigned long StartOnDemand = 3;

	static constexpr auto LinuxUdevRuleFile = "/etc/udev/rules.d/10-veyon-ifpr-usb-storage.rules";

};
