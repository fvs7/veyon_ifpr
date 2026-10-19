/*
 * UsbStorageControl.cpp - implementation of UsbStorageControl class
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

#ifdef _WIN32
#include <windows.h>
#endif

#include "UsbStorageControl.h"
#include "VeyonCore.h"

#ifdef Q_OS_LINUX
#include <QFile>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#endif


#ifdef _WIN32

namespace {

constexpr auto UsbStorKey = L"SYSTEM\\CurrentControlSet\\Services\\USBSTOR";
constexpr auto UsbStorValue = L"Start";
constexpr auto BackupKey = L"SOFTWARE\\Veyon IFPR\\ExamMode";
constexpr auto BackupValue = L"UsbStorOriginalStart";

// always operate on the native (64 bit) registry view, even in 32 bit builds
constexpr REGSAM RegistryView = KEY_WOW64_64KEY;


class RegistryKey
{
public:
	RegistryKey(const wchar_t* path, REGSAM access, bool create = false)
	{
		const auto result = create ?
			RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, nullptr, REG_OPTION_NON_VOLATILE,
							access | RegistryView, nullptr, &m_key, nullptr) :
			RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, access | RegistryView, &m_key);
		if (result != ERROR_SUCCESS)
		{
			m_key = nullptr;
		}
	}

	~RegistryKey()
	{
		if (m_key)
		{
			RegCloseKey(m_key);
		}
	}

	bool isValid() const
	{
		return m_key != nullptr;
	}

	bool readDword(const wchar_t* name, DWORD* value) const
	{
		DWORD type = 0;
		DWORD size = sizeof(DWORD);
		return m_key &&
			   RegQueryValueExW(m_key, name, nullptr, &type, reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS &&
			   type == REG_DWORD;
	}

	bool writeDword(const wchar_t* name, DWORD value)
	{
		return m_key &&
			   RegSetValueExW(m_key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)) == ERROR_SUCCESS;
	}

	bool deleteValue(const wchar_t* name)
	{
		return m_key && RegDeleteValueW(m_key, name) == ERROR_SUCCESS;
	}

	Q_DISABLE_COPY(RegistryKey)

private:
	HKEY m_key = nullptr;
};

} // namespace



bool UsbStorageControl::disable()
{
	RegistryKey usbStor(UsbStorKey, KEY_QUERY_VALUE | KEY_SET_VALUE);
	if (usbStor.isValid() == false)
	{
		vCritical() << "could not open USBSTOR service key (missing privileges or driver not installed)";
		return false;
	}

	DWORD currentStart = StartOnDemand;
	if (usbStor.readDword(UsbStorValue, &currentStart) == false)
	{
		vWarning() << "could not read USBSTOR Start value - assuming" << StartOnDemand;
		currentStart = StartOnDemand;
	}

	// back up the original value only once, so repeated "start exam" commands do not
	// overwrite the real original value with 4
	if (isDisabledByExamMode() == false)
	{
		RegistryKey backup(BackupKey, KEY_QUERY_VALUE | KEY_SET_VALUE, true);
		if (backup.writeDword(BackupValue, currentStart) == false)
		{
			vCritical() << "could not back up original USBSTOR Start value";
			return false;
		}
	}

	if (usbStor.writeDword(UsbStorValue, StartDisabled) == false)
	{
		vCritical() << "could not disable USBSTOR";
		return false;
	}

	vInfo() << "USB mass storage disabled (previous Start value:" << currentStart << ")";
	return true;
}



bool UsbStorageControl::restore()
{
	RegistryKey backup(BackupKey, KEY_QUERY_VALUE | KEY_SET_VALUE);

	DWORD originalStart = StartOnDemand;
	if (backup.readDword(BackupValue, &originalStart) == false)
	{
		// exam mode never changed USBSTOR on this computer - leave the administrator's setting alone
		vInfo() << "no USBSTOR backup found - nothing to restore";
		return true;
	}

	RegistryKey usbStor(UsbStorKey, KEY_SET_VALUE);
	if (usbStor.writeDword(UsbStorValue, originalStart) == false)
	{
		vCritical() << "could not restore USBSTOR Start value";
		return false;
	}

	backup.deleteValue(BackupValue);

	vInfo() << "USB mass storage restored (Start value:" << originalStart << ")";
	return true;
}



bool UsbStorageControl::isDisabledByExamMode()
{
	DWORD value = 0;
	return RegistryKey(BackupKey, KEY_QUERY_VALUE).readDword(BackupValue, &value);
}

#elif defined(Q_OS_LINUX)

namespace {

constexpr int UdevadmTimeout = 10000;

bool reloadUdevRules()
{
	auto udevadm = QStandardPaths::findExecutable(QStringLiteral("udevadm"),
												  {QStringLiteral("/usr/bin"), QStringLiteral("/bin"),
												   QStringLiteral("/sbin")});
	if (udevadm.isEmpty())
	{
		vCritical() << "udevadm not found";
		return false;
	}

	QProcess process;
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.start(udevadm, {QStringLiteral("control"), QStringLiteral("--reload-rules")});
	if (process.waitForFinished(UdevadmTimeout) == false ||
		process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
	{
		vCritical() << "udevadm control --reload-rules failed:" << process.readAll().trimmed();
		return false;
	}

	return true;
}

} // namespace



bool UsbStorageControl::disable()
{
	// deauthorize every new USB interface of class 08 (mass storage) - works regardless
	// of whether the usb-storage/uas modules are already loaded
	QSaveFile ruleFile(QString::fromLatin1(LinuxUdevRuleFile));
	if (ruleFile.open(QFile::WriteOnly | QFile::Truncate) == false ||
		ruleFile.write("# Veyon IFPR - Modo Prova: bloqueia pendrives/midias USB (removido ao finalizar a prova)\n"
					   "ACTION==\"add\", SUBSYSTEM==\"usb\", ENV{DEVTYPE}==\"usb_interface\", "
					   "ATTR{bInterfaceClass}==\"08\", ATTR{authorized}=\"0\"\n") < 0 ||
		ruleFile.commit() == false)
	{
		vCritical() << "could not write" << LinuxUdevRuleFile;
		return false;
	}

	if (reloadUdevRules() == false)
	{
		return false;
	}

	vInfo() << "USB mass storage disabled (udev rule" << LinuxUdevRuleFile << ")";
	return true;
}



bool UsbStorageControl::restore()
{
	const auto ruleFilePath = QString::fromLatin1(LinuxUdevRuleFile);
	if (QFile::exists(ruleFilePath) == false)
	{
		vInfo() << "no exam mode udev rule found - nothing to restore";
		return true;
	}

	if (QFile::remove(ruleFilePath) == false)
	{
		vCritical() << "could not remove" << ruleFilePath;
		return false;
	}

	reloadUdevRules();

	vInfo() << "USB mass storage restored";
	return true;
}



bool UsbStorageControl::isDisabledByExamMode()
{
	return QFile::exists(QString::fromLatin1(LinuxUdevRuleFile));
}

#else

bool UsbStorageControl::disable()
{
	vWarning() << "USB storage control is only implemented on Windows";
	return false;
}



bool UsbStorageControl::restore()
{
	return false;
}



bool UsbStorageControl::isDisabledByExamMode()
{
	return false;
}

#endif
