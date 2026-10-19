/*
 * ExamModeFeaturePlugin.h - declaration of ExamModeFeaturePlugin class
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

class ExamModeOverlay;

class ExamModeFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.ExamMode")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit ExamModeFeaturePlugin(QObject* parent = nullptr);
	~ExamModeFeaturePlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{QStringLiteral("3ffe3966-3937-47d5-9425-03ca77cd5c17")};
	}

	QVersionNumber version() const override
	{
		return QVersionNumber(1, 0);
	}

	QString name() const override
	{
		return QStringLiteral("ExamMode");
	}

	QString description() const override
	{
		return tr("Modo Prova: bloqueia Internet e pendrives e exibe um aviso na tela do aluno");
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

	bool handleFeatureMessage(VeyonWorkerInterface& worker, const FeatureMessage& message) override;

	void sendAsyncFeatureMessages(VeyonServerInterface& server, const MessageContext& messageContext) override;

	bool isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const override;

private:
	enum class WorkerCommand
	{
		ShowOverlay,
		HideOverlay
	};

	static constexpr int StateCacheTimeout = 5000;
	static constexpr int OverlayRetryInterval = 15000;

	void startExam(VeyonServerInterface& server);
	void finishExam(VeyonServerInterface& server);
	void showOverlay(VeyonServerInterface& server);
	bool isExamActive() const;

	const Feature m_startExamFeature;
	const Feature m_finishExamFeature;
	const FeatureList m_features;

	QList<ExamModeOverlay*> m_overlays;

	mutable bool m_examActive = false;
	mutable QElapsedTimer m_stateTimer;
	QElapsedTimer m_overlayRetryTimer;

};
