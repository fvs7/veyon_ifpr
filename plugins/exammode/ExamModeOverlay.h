/*
 * ExamModeOverlay.h - declaration of ExamModeOverlay class
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

#include <QTimer>
#include <QWidget>

class QScreen;

/*!
 * \brief Small, click-through banner shown at the top of each screen while exam mode is active
 */
class ExamModeOverlay : public QWidget
{
	Q_OBJECT
public:
	explicit ExamModeOverlay(QScreen* screen, const QString& text);
	~ExamModeOverlay() override = default;

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	static constexpr int BannerHeight = 34;
	static constexpr int MaximumBannerWidth = 760;
	static constexpr int RaiseInterval = 2000;

	void updateGeometryForScreen();

	QScreen* m_screen;
	QString m_text;
	QTimer m_raiseTimer;

};
