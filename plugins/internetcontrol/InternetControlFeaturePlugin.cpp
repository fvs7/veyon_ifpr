/*
 * InternetControlFeaturePlugin.cpp - implementation of InternetControlFeaturePlugin class
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

#include "ComputerControlInterface.h"
#include "InternetAccessBlocker.h"
#include "InternetControlFeaturePlugin.h"
#include "VeyonServerInterface.h"


InternetControlFeaturePlugin::InternetControlFeaturePlugin(QObject* parent) :
	QObject(parent),
	m_blockInternetFeature(QStringLiteral("BlockInternet"),
						   Feature::Flag::Action | Feature::Flag::AllComponents,
						   Feature::Uid("701f990e-a4bb-4a6f-b897-97a4ffd9236e"),
						   Feature::Uid(),
						   tr("Bloquear Internet"), {},
						   tr("Bloqueia o acesso à Internet (HTTP/HTTPS) nos computadores selecionados. "
							  "A rede local e a comunicação com o Veyon continuam funcionando."),
						   QStringLiteral(":/internetcontrol/internet-block.png")),
	m_allowInternetFeature(QStringLiteral("AllowInternet"),
						   Feature::Flag::Action | Feature::Flag::AllComponents,
						   Feature::Uid("7c385824-76f3-4596-a1fa-bfe5e8c96569"),
						   Feature::Uid(),
						   tr("Liberar Internet"), {},
						   tr("Libera novamente o acesso à Internet nos computadores selecionados."),
						   QStringLiteral(":/internetcontrol/internet-allow.png")),
	m_features({m_blockInternetFeature, m_allowInternetFeature})
{
}



bool InternetControlFeaturePlugin::controlFeature(Feature::Uid featureUid, Operation operation,
												  const QVariantMap& arguments,
												  const ComputerControlInterfaceList& computerControlInterfaces)
{
	Q_UNUSED(arguments)

	if (hasFeature(featureUid) == false || operation != Operation::Start)
	{
		return false;
	}

	if (featureUid == m_blockInternetFeature.uid())
	{
		// never block the teacher's own computer, even if it is listed in the room
		auto targets = computerControlInterfaces;
		targets.removeLocalHostInterfaces();
		sendFeatureMessage(FeatureMessage{featureUid}, targets);
		return true;
	}

	sendFeatureMessage(FeatureMessage{featureUid}, computerControlInterfaces);
	return true;
}



bool InternetControlFeaturePlugin::handleFeatureMessage(VeyonServerInterface& server,
														const MessageContext& messageContext,
														const FeatureMessage& message)
{
	Q_UNUSED(server)
	Q_UNUSED(messageContext)

	if (message.featureUid() == m_blockInternetFeature.uid())
	{
		updateState(InternetAccessBlocker::block());
		return true;
	}

	if (message.featureUid() == m_allowInternetFeature.uid())
	{
		updateState(InternetAccessBlocker::unblock() == false);
		return true;
	}

	return false;
}



bool InternetControlFeaturePlugin::isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const
{
	Q_UNUSED(server)

	if (featureUid != m_blockInternetFeature.uid())
	{
		return false;
	}

	if (m_stateTimer.isValid() == false || m_stateTimer.hasExpired(StateCacheTimeout))
	{
		updateState(InternetAccessBlocker::isBlocked());
	}

	return m_blocked;
}



void InternetControlFeaturePlugin::updateState(bool blocked) const
{
	m_blocked = blocked;
	m_stateTimer.start();
}
