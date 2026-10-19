/*
 * InternetControlFeaturePlugin.h - declaration of InternetControlFeaturePlugin class
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

#include <QElapsedTimer>

#include "FeatureProviderInterface.h"

class InternetControlFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.InternetControl")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit InternetControlFeaturePlugin(QObject* parent = nullptr);
	~InternetControlFeaturePlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{QStringLiteral("0a7f89be-f6f2-4f57-bb15-4b75d5e67241")};
	}

	QVersionNumber version() const override
	{
		return QVersionNumber(1, 0);
	}

	QString name() const override
	{
		return QStringLiteral("InternetControl");
	}

	QString description() const override
	{
		return tr("Bloquear e liberar o acesso à Internet (HTTP/HTTPS) nos computadores dos alunos");
	}

	QString vendor() const override
	{
		return QStringLiteral("IFPR - Campus União da Vitória (REA)");
	}

	QString copyright() const override
	{
		return QStringLiteral("IFPR - GNU GPL v2");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature(Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool handleFeatureMessage(VeyonServerInterface& server,
							  const MessageContext& messageContext,
							  const FeatureMessage& message) override;

	bool isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const override;

private:
	const Feature m_blockInternetFeature;
	const Feature m_allowInternetFeature;
	const FeatureList m_features;

	// cached firewall state on the client side - the firewall rules may also be changed
	// by the ExamMode plugin, so the cache expires after a few seconds
	static constexpr int StateCacheTimeout = 5000;
	mutable bool m_blocked = false;
	mutable QElapsedTimer m_stateTimer;

	void updateState(bool blocked) const;

};
