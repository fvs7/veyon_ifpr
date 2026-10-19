/*
 * InternetAccessBlocker.h - declaration of InternetAccessBlocker class
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

#include <QString>

/*!
 * \brief Blocks/unblocks outgoing web traffic (HTTP/HTTPS) to the Internet
 *
 * On Windows this is implemented with outbound block rules in the Windows
 * Defender Firewall (INetFwPolicy2). The rules only match *remote* ports
 * 80/tcp, 443/tcp and 443/udp (QUIC/HTTP3) towards *public* addresses, so:
 *
 *  - local Veyon traffic (port 11100 and the other Veyon ports) is never matched
 *  - private networks (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16), loopback
 *    and link-local addresses are never matched
 *
 * The rules are persistent (they survive reboots) until unblock() is called.
 * All methods must be called from a process with administrative privileges
 * (the Veyon Server runs as SYSTEM on Windows).
 *
 * On Linux the same rules are implemented with nftables in a dedicated table
 * (inet veyon_ifpr, output hook), which does not interfere with ufw or other
 * firewall tables. The ruleset is stored in LinuxStateFile and re-applied at boot
 * by veyon-ifpr-restore.service. This requires root privileges, i.e. the Veyon
 * Server must run as root, which is the case for X11 sessions (e.g. Xubuntu).
 *
 * On other platforms all methods are no-ops which return false.
 */
class InternetAccessBlocker
{
public:
	static bool block();
	static bool unblock();
	static bool isBlocked();

	//! remote IPv4/IPv6 ranges considered "Internet" (everything except private/local ranges)
	static QString internetAddressRanges();

	static constexpr auto RuleGroup = "Veyon IFPR";
	static constexpr auto RuleNameTcp = "Veyon IFPR - Bloqueio de Internet (TCP 80/443)";
	static constexpr auto RuleNameUdp = "Veyon IFPR - Bloqueio de Internet (UDP 443/QUIC)";

	static constexpr auto LinuxStateFile = "/var/lib/veyon-ifpr/internet-block.nft";

};
