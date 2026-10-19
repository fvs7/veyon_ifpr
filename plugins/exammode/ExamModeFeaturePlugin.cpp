/*
 * ExamModeFeaturePlugin.cpp - implementation of ExamModeFeaturePlugin class
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

#include <QCoreApplication>
#include <QGuiApplication>

#include "ComputerControlInterface.h"
#include "ExamModeFeaturePlugin.h"
#include "ExamModeOverlay.h"
#include "FeatureWorkerManager.h"
#include "InternetAccessBlocker.h"
#include "PlatformPluginInterface.h"
#include "PlatformSessionFunctions.h"
#include "UsbStorageControl.h"
#include "VeyonServerInterface.h"


ExamModeFeaturePlugin::ExamModeFeaturePlugin(QObject* parent) :
	QObject(parent),
	m_startExamFeature(QStringLiteral("StartExam"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid("e1b2bf25-1880-4e39-b297-a13561008540"),
					   Feature::Uid(),
					   tr("Iniciar Modo Prova"), {},
					   tr("Inicia o Modo Prova nos computadores selecionados: bloqueia a Internet, "
						  "bloqueia pendrives/mídias removíveis e exibe um aviso no topo da tela do aluno."),
					   QStringLiteral(":/exammode/exam-start.png")),
	m_finishExamFeature(QStringLiteral("FinishExam"),
						Feature::Flag::Action | Feature::Flag::AllComponents,
						Feature::Uid("31c522ad-106b-4089-8938-1dd97f67e8e3"),
						Feature::Uid(),
						tr("Finalizar Prova"), {},
						tr("Finaliza o Modo Prova: libera a Internet e os pendrives e remove o aviso da tela."),
						QStringLiteral(":/exammode/exam-stop.png")),
	m_features({m_startExamFeature, m_finishExamFeature})
{
}



ExamModeFeaturePlugin::~ExamModeFeaturePlugin()
{
	qDeleteAll(m_overlays);
}



bool ExamModeFeaturePlugin::controlFeature(Feature::Uid featureUid, Operation operation,
										   const QVariantMap& arguments,
										   const ComputerControlInterfaceList& computerControlInterfaces)
{
	Q_UNUSED(arguments)

	if (hasFeature(featureUid) == false || operation != Operation::Start)
	{
		return false;
	}

	if (featureUid == m_startExamFeature.uid())
	{
		// never put the teacher's own computer into exam mode
		auto targets = computerControlInterfaces;
		targets.removeLocalHostInterfaces();
		sendFeatureMessage(FeatureMessage{featureUid}, targets);
		return true;
	}

	sendFeatureMessage(FeatureMessage{featureUid}, computerControlInterfaces);
	return true;
}



bool ExamModeFeaturePlugin::handleFeatureMessage(VeyonServerInterface& server,
												 const MessageContext& messageContext,
												 const FeatureMessage& message)
{
	Q_UNUSED(messageContext)

	if (message.featureUid() == m_startExamFeature.uid())
	{
		startExam(server);
		return true;
	}

	if (message.featureUid() == m_finishExamFeature.uid())
	{
		finishExam(server);
		return true;
	}

	return false;
}



bool ExamModeFeaturePlugin::handleFeatureMessage(VeyonWorkerInterface& worker, const FeatureMessage& message)
{
	Q_UNUSED(worker)

	if (message.featureUid() != m_startExamFeature.uid())
	{
		return false;
	}

	switch (message.command<WorkerCommand>())
	{
	case WorkerCommand::ShowOverlay:
		if (m_overlays.isEmpty())
		{
			const auto text = tr("MODO EXAME / PROVA  •  Internet e pendrives bloqueados pelo professor");
			const auto screens = QGuiApplication::screens();
			for (auto* screen : screens)
			{
				m_overlays.append(new ExamModeOverlay(screen, text));
			}
		}
		return true;

	case WorkerCommand::HideOverlay:
		qDeleteAll(m_overlays);
		m_overlays.clear();
		QCoreApplication::quit();
		return true;
	}

	return false;
}



void ExamModeFeaturePlugin::sendAsyncFeatureMessages(VeyonServerInterface& server, const MessageContext& messageContext)
{
	Q_UNUSED(messageContext)

	// self-healing: if the computer was rebooted (or the user logged in again) while the
	// exam is still active, the restrictions persist but the overlay is gone - show it again
	if (isExamActive() &&
		server.featureWorkerManager().isWorkerRunning(m_startExamFeature.uid()) == false &&
		(m_overlayRetryTimer.isValid() == false || m_overlayRetryTimer.hasExpired(OverlayRetryInterval)))
	{
		m_overlayRetryTimer.start();
		showOverlay(server);
	}
}



bool ExamModeFeaturePlugin::isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const
{
	Q_UNUSED(server)

	return featureUid == m_startExamFeature.uid() && isExamActive();
}



void ExamModeFeaturePlugin::startExam(VeyonServerInterface& server)
{
	vInfo() << "starting exam mode";

	const auto internetBlocked = InternetAccessBlocker::block();
	const auto usbDisabled = UsbStorageControl::disable();

	if (internetBlocked == false || usbDisabled == false)
	{
		vCritical() << "exam mode started with errors - internet blocked:" << internetBlocked
					<< "USB storage disabled:" << usbDisabled;
	}

	m_stateTimer.invalidate();
	m_overlayRetryTimer.start();

	showOverlay(server);
}



void ExamModeFeaturePlugin::finishExam(VeyonServerInterface& server)
{
	vInfo() << "finishing exam mode";

	InternetAccessBlocker::unblock();
	UsbStorageControl::restore();

	m_stateTimer.invalidate();

	if (server.featureWorkerManager().isWorkerRunning(m_startExamFeature.uid()))
	{
		server.featureWorkerManager().sendMessageToManagedSystemWorker(
			FeatureMessage{m_startExamFeature.uid(), WorkerCommand::HideOverlay});
	}
}



void ExamModeFeaturePlugin::showOverlay(VeyonServerInterface& server)
{
	if (VeyonCore::platform().sessionFunctions().currentSessionHasUser() == false)
	{
		vDebug() << "not showing exam overlay since not running in a user session";
		return;
	}

	server.featureWorkerManager().sendMessageToManagedSystemWorker(
		FeatureMessage{m_startExamFeature.uid(), WorkerCommand::ShowOverlay});
}



bool ExamModeFeaturePlugin::isExamActive() const
{
	if (m_stateTimer.isValid() == false || m_stateTimer.hasExpired(StateCacheTimeout))
	{
		m_examActive = UsbStorageControl::isDisabledByExamMode();
		m_stateTimer.start();
	}

	return m_examActive;
}
