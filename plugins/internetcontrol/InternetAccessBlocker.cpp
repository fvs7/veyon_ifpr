/*
 * InternetAccessBlocker.cpp - implementation of InternetAccessBlocker class
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

// Windows headers must come first so that INITGUID instantiates the firewall GUIDs
#ifdef _WIN32
#define INITGUID
#include <winsock2.h>
#include <windows.h>
#include <oleauto.h>
#include "../platform/windows/netfw.h"
#endif

#include "InternetAccessBlocker.h"
#include "VeyonCore.h"

#ifdef Q_OS_LINUX
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#endif


QString InternetAccessBlocker::internetAddressRanges()
{
	// Every routable unicast address EXCEPT:
	//   0.0.0.0/8, 10.0.0.0/8 (private), 127.0.0.0/8 (loopback), 169.254.0.0/16 (link-local),
	//   172.16.0.0/12 (private), 192.168.0.0/16 (private), 224.0.0.0/3 (multicast/reserved)
	// plus IPv6 global unicast (2000::/3). IPv6 ULA (fc00::/7) and link-local (fe80::/10)
	// are not included and therefore never blocked.
	return QStringLiteral(
		"1.0.0.0-9.255.255.255,"
		"11.0.0.0-126.255.255.255,"
		"128.0.0.0-169.253.255.255,"
		"169.255.0.0-172.15.255.255,"
		"172.32.0.0-192.167.255.255,"
		"192.169.0.0-223.255.255.255,"
		"2000::-3fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
}



#ifdef _WIN32

namespace {

class ComInitializer
{
public:
	ComInitializer() :
		m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))
	{
	}

	~ComInitializer()
	{
		if (SUCCEEDED(m_result))
		{
			CoUninitialize();
		}
	}

	// RPC_E_CHANGED_MODE: COM already initialized in a different mode, which is fine for us
	bool isUsable() const
	{
		return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE;
	}

	Q_DISABLE_COPY(ComInitializer)

private:
	HRESULT m_result;
};


class Bstr
{
public:
	explicit Bstr(const QString& string) :
		m_bstr(SysAllocString(reinterpret_cast<const OLECHAR*>(string.utf16())))
	{
	}

	~Bstr()
	{
		SysFreeString(m_bstr);
	}

	operator BSTR() const
	{
		return m_bstr;
	}

	Q_DISABLE_COPY(Bstr)

private:
	BSTR m_bstr;
};


template<class T>
class ComPtr
{
public:
	ComPtr() = default;
	~ComPtr()
	{
		if (m_ptr)
		{
			m_ptr->Release();
		}
	}

	T* operator->() const { return m_ptr; }
	T* get() const { return m_ptr; }
	T** put() { return &m_ptr; }
	void** putVoid() { return reinterpret_cast<void**>(&m_ptr); }

	Q_DISABLE_COPY(ComPtr)

private:
	T* m_ptr = nullptr;
};



template<class Callback>
bool withFirewallRules(Callback&& callback)
{
	ComInitializer com;
	if (com.isUsable() == false)
	{
		vCritical() << "could not initialize COM";
		return false;
	}

	ComPtr<INetFwPolicy2> policy;
	auto hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_INPROC_SERVER, IID_INetFwPolicy2, policy.putVoid());
	if (FAILED(hr))
	{
		vCritical() << "CoCreateInstance(NetFwPolicy2) failed" << Qt::hex << hr
					<< "- is the Windows Defender Firewall service (MpsSvc) running?";
		return false;
	}

	ComPtr<INetFwRules> rules;
	hr = policy->get_Rules(rules.put());
	if (FAILED(hr))
	{
		vCritical() << "INetFwPolicy2::get_Rules() failed" << Qt::hex << hr;
		return false;
	}

	return callback(policy.get(), rules.get());
}



bool ruleExists(INetFwRules* rules, const QString& name)
{
	ComPtr<INetFwRule> rule;
	return SUCCEEDED(rules->Item(Bstr(name), rule.put())) && rule.get();
}



void removeRule(INetFwRules* rules, const QString& name)
{
	// several rules may share the same name (e.g. after a crash) - remove all of them
	for (int i = 0; i < 32 && ruleExists(rules, name); ++i)
	{
		const auto hr = rules->Remove(Bstr(name));
		if (FAILED(hr))
		{
			vWarning() << "INetFwRules::Remove() failed for" << name << Qt::hex << hr;
			return;
		}
	}
}



bool addBlockRule(INetFwRules* rules, const QString& name, NET_FW_IP_PROTOCOL protocol, const QString& remotePorts)
{
	ComPtr<INetFwRule> rule;
	auto hr = CoCreateInstance(CLSID_NetFwRule, nullptr, CLSCTX_INPROC_SERVER, IID_INetFwRule, rule.putVoid());
	if (FAILED(hr))
	{
		vCritical() << "CoCreateInstance(NetFwRule) failed" << Qt::hex << hr;
		return false;
	}

	const Bstr ruleName(name);
	const Bstr description(QStringLiteral("Regra criada pelo Veyon IFPR (Controle de Internet / Modo Prova). "
										   "Bloqueia apenas HTTP/HTTPS para enderecos publicos; rede local e Veyon (11100) nao sao afetados."));
	const Bstr grouping(QString::fromLatin1(InternetAccessBlocker::RuleGroup));
	const Bstr ports(remotePorts);
	const Bstr addresses(InternetAccessBlocker::internetAddressRanges());

	// NOTE: protocol has to be set before ports, otherwise put_RemotePorts() fails
	const HRESULT results[] = {
		rule->put_Name(ruleName),
		rule->put_Description(description),
		rule->put_Grouping(grouping),
		rule->put_Direction(NET_FW_RULE_DIR_OUT),
		rule->put_Protocol(protocol),
		rule->put_RemotePorts(ports),
		rule->put_RemoteAddresses(addresses),
		rule->put_Action(NET_FW_ACTION_BLOCK),
		rule->put_Profiles(NET_FW_PROFILE2_ALL),
		rule->put_Enabled(VARIANT_TRUE),
	};

	for (const auto result : results)
	{
		if (FAILED(result))
		{
			vCritical() << "failed to configure firewall rule" << name << Qt::hex << result;
			return false;
		}
	}

	hr = rules->Add(rule.get());
	if (FAILED(hr))
	{
		vCritical() << "INetFwRules::Add() failed for" << name << Qt::hex << hr;
		return false;
	}

	return true;
}



void warnIfFirewallDisabled(INetFwPolicy2* policy)
{
	long currentProfiles = 0;
	if (FAILED(policy->get_CurrentProfileTypes(&currentProfiles)))
	{
		return;
	}

	for (const auto profile : {NET_FW_PROFILE2_DOMAIN, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_PUBLIC})
	{
		VARIANT_BOOL enabled = VARIANT_TRUE;
		if ((currentProfiles & profile) &&
			SUCCEEDED(policy->get_FirewallEnabled(profile, &enabled)) &&
			enabled == VARIANT_FALSE)
		{
			vCritical() << "Windows Defender Firewall is DISABLED for the active profile" << profile
						<< "- Internet blocking rules will have no effect!";
		}
	}
}

} // namespace



bool InternetAccessBlocker::block()
{
	return withFirewallRules([](INetFwPolicy2* policy, INetFwRules* rules) {
		warnIfFirewallDisabled(policy);

		const auto tcpName = QString::fromUtf8(RuleNameTcp);
		const auto udpName = QString::fromUtf8(RuleNameUdp);

		// make the operation idempotent
		removeRule(rules, tcpName);
		removeRule(rules, udpName);

		const auto success = addBlockRule(rules, tcpName, NET_FW_IP_PROTOCOL_TCP, QStringLiteral("80,443")) &&
							 addBlockRule(rules, udpName, NET_FW_IP_PROTOCOL_UDP, QStringLiteral("443"));
		if (success == false)
		{
			// never leave a half-configured state behind
			removeRule(rules, tcpName);
			removeRule(rules, udpName);
			return false;
		}

		vInfo() << "Internet access blocked (HTTP/HTTPS to public addresses)";
		return true;
	});
}



bool InternetAccessBlocker::unblock()
{
	return withFirewallRules([](INetFwPolicy2*, INetFwRules* rules) {
		const auto tcpName = QString::fromUtf8(RuleNameTcp);
		const auto udpName = QString::fromUtf8(RuleNameUdp);

		removeRule(rules, tcpName);
		removeRule(rules, udpName);

		const auto success = ruleExists(rules, tcpName) == false && ruleExists(rules, udpName) == false;
		if (success)
		{
			vInfo() << "Internet access unblocked";
		}
		return success;
	});
}



bool InternetAccessBlocker::isBlocked()
{
	return withFirewallRules([](INetFwPolicy2*, INetFwRules* rules) {
		return ruleExists(rules, QString::fromUtf8(RuleNameTcp));
	});
}

#elif defined(Q_OS_LINUX)

namespace {

constexpr auto NftFamily = "inet";
constexpr auto NftTable = "veyon_ifpr";
constexpr int NftTimeout = 10000;


QString nftPath()
{
	const auto path = QStandardPaths::findExecutable(QStringLiteral("nft"),
													 {QStringLiteral("/usr/sbin"), QStringLiteral("/sbin")});
	return path.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("nft")) : path;
}



bool runNft(const QStringList& arguments, bool logErrors = true)
{
	const auto nft = nftPath();
	if (nft.isEmpty())
	{
		vCritical() << "nft not found - please install the nftables package";
		return false;
	}

	QProcess process;
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.start(nft, arguments);
	if (process.waitForFinished(NftTimeout) == false)
	{
		process.kill();
		process.waitForFinished();
		vCritical() << "nft" << arguments << "timed out";
		return false;
	}

	if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
	{
		if (logErrors)
		{
			vCritical() << "nft" << arguments << "failed:" << process.readAll().trimmed();
		}
		return false;
	}

	return true;
}



bool tableExists()
{
	return runNft({QStringLiteral("list"), QStringLiteral("table"),
				   QString::fromLatin1(NftFamily), QString::fromLatin1(NftTable)}, false);
}



bool loadStateFile()
{
	return runNft({QStringLiteral("-f"), QString::fromLatin1(InternetAccessBlocker::LinuxStateFile)});
}



// Same address ranges as internetAddressRanges(), expressed as exclusions: everything
// except private/local IPv4 ranges and everything except IPv6 global unicast (2000::/3)
// is accepted. "table + delete table" makes loading the file idempotent (atomic replace).
QString ruleset()
{
	return QStringLiteral(
		"# Veyon IFPR - Internet blocking (generated, removed by \"Liberar Internet\")\n"
		"table inet veyon_ifpr\n"
		"delete table inet veyon_ifpr\n"
		"table inet veyon_ifpr {\n"
		"\tchain output {\n"
		"\t\ttype filter hook output priority filter; policy accept;\n"
		"\t\tip daddr { 0.0.0.0/8, 10.0.0.0/8, 127.0.0.0/8, 169.254.0.0/16, 172.16.0.0/12, "
		"192.168.0.0/16, 224.0.0.0/3 } accept\n"
		"\t\tip6 daddr != 2000::/3 accept\n"
		"\t\ttcp dport { 80, 443 } reject with tcp reset\n"
		"\t\tudp dport 443 reject\n"
		"\t}\n"
		"}\n");
}

} // namespace



bool InternetAccessBlocker::block()
{
	const QFileInfo stateFileInfo(QString::fromLatin1(LinuxStateFile));
	if (QDir().mkpath(stateFileInfo.absolutePath()) == false)
	{
		vCritical() << "could not create" << stateFileInfo.absolutePath();
		return false;
	}

	// the state file is also loaded by veyon-ifpr-restore.service at boot time
	QSaveFile stateFile(stateFileInfo.absoluteFilePath());
	if (stateFile.open(QFile::WriteOnly | QFile::Truncate) == false ||
		stateFile.write(ruleset().toUtf8()) < 0 ||
		stateFile.commit() == false)
	{
		vCritical() << "could not write" << stateFileInfo.absoluteFilePath();
		return false;
	}

	if (loadStateFile() == false)
	{
		// never leave a half-configured state behind
		unblock();
		return false;
	}

	vInfo() << "Internet access blocked (HTTP/HTTPS to public addresses)";
	return true;
}



bool InternetAccessBlocker::unblock()
{
	QFile::remove(QString::fromLatin1(LinuxStateFile));

	if (tableExists())
	{
		runNft({QStringLiteral("delete"), QStringLiteral("table"),
				QString::fromLatin1(NftFamily), QString::fromLatin1(NftTable)});
	}

	const auto success = tableExists() == false;
	if (success)
	{
		vInfo() << "Internet access unblocked";
	}
	return success;
}



bool InternetAccessBlocker::isBlocked()
{
	// self-healing: re-apply the rules if they got lost (e.g. "nft flush ruleset")
	if (QFile::exists(QString::fromLatin1(LinuxStateFile)) && tableExists() == false)
	{
		vWarning() << "Internet blocking rules missing - re-applying";
		loadStateFile();
	}

	return tableExists();
}

#else

bool InternetAccessBlocker::block()
{
	vWarning() << "Internet blocking is only implemented on Windows";
	return false;
}



bool InternetAccessBlocker::unblock()
{
	vWarning() << "Internet blocking is only implemented on Windows";
	return false;
}



bool InternetAccessBlocker::isBlocked()
{
	return false;
}

#endif
