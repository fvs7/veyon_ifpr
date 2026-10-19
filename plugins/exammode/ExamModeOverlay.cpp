/*
 * ExamModeOverlay.cpp - implementation of ExamModeOverlay class
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

#include <QPainter>
#include <QPainterPath>
#include <QScreen>

#include "ExamModeOverlay.h"


ExamModeOverlay::ExamModeOverlay(QScreen* screen, const QString& text) :
	QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool |
					 Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus),
	m_screen(screen),
	m_text(text)
{
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setAttribute(Qt::WA_ShowWithoutActivating);
	setAttribute(Qt::WA_DeleteOnClose, false);

	auto font = this->font();
	font.setBold(true);
	font.setPointSize(11);
	setFont(font);

	if (m_screen)
	{
		setScreen(m_screen);
		connect(m_screen, &QScreen::geometryChanged, this, &ExamModeOverlay::updateGeometryForScreen);
	}
	updateGeometryForScreen();

	// other "always on top" windows (e.g. fullscreen browsers) may cover the banner - keep raising it
	connect(&m_raiseTimer, &QTimer::timeout, this, [this]() { raise(); });
	m_raiseTimer.start(RaiseInterval);

	show();
	raise();
}



void ExamModeOverlay::paintEvent(QPaintEvent* event)
{
	Q_UNUSED(event)

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// rounded only at the bottom, so the banner looks attached to the top screen edge
	QPainterPath path;
	const QRectF r = rect();
	const qreal radius = 10;
	path.moveTo(r.topLeft());
	path.lineTo(r.topRight());
	path.lineTo(r.right(), r.bottom() - radius);
	path.quadTo(r.bottomRight(), QPointF(r.right() - radius, r.bottom()));
	path.lineTo(r.left() + radius, r.bottom());
	path.quadTo(r.bottomLeft(), QPointF(r.left(), r.bottom() - radius));
	path.closeSubpath();

	painter.fillPath(path, QColor(176, 84, 0, 225));

	painter.setPen(Qt::white);
	painter.drawText(rect(), Qt::AlignCenter, m_text);
}



void ExamModeOverlay::updateGeometryForScreen()
{
	const auto screenGeometry = m_screen ? m_screen->geometry() : QRect(0, 0, 1024, 768);
	const auto width = qMin(screenGeometry.width(), MaximumBannerWidth);

	setGeometry(screenGeometry.x() + (screenGeometry.width() - width) / 2,
				screenGeometry.y(),
				width, BannerHeight);
}
