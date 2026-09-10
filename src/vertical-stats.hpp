/*
Vertical Stats for OBS
Copyright (C) 2026 Nathan V

Derived from OBS Studio's built-in Stats panel (frontend/widgets/OBSBasicStats.cpp),
Copyright (C) 2023 by Lain Bailey, licensed under the GNU GPL v2 or later.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include <QFrame>
#include <QMap>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <string>
#include <vector>

#include "stats-logic.hpp"

class QLabel;
class QGridLayout;
class QVBoxLayout;
class QWidget;

/*
 * A narrow, top-to-bottom version of the OBS Stats panel, meant to live in a
 * side dock without forcing the preview to shrink.
 *
 * Layout:
 *   (no heading)     CPU, FPS, Render time, Memory, Disk free, Disk full in
 *   Lost Frames      Render, Encoding, Network (main stream)
 *   Stream           status, Sent, Bitrate
 *   Recording        status, Sent, Bitrate
 *   Vertical Stream… status, Sent, Bitrate, Network   (Aitum Vertical only)
 *   Vertical Recording                                 (Aitum Vertical only)
 *   [Reset]
 *
 * All the arithmetic lives in stats-logic.hpp; this class only reads libobs
 * and paints labels.
 */
class VerticalStats : public QFrame {
	Q_OBJECT

public:
	explicit VerticalStats(QWidget *parent = nullptr);
	~VerticalStats() override;

public slots:
	void Reset();

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private slots:
	void Update();
	void RecordingTimeLeft();

private:
	/* One block of output statistics (Stream, Recording, Vertical …). */
	struct OutputBlock {
		QPointer<QWidget> frame;
		QPointer<QLabel> title;
		QPointer<QLabel> status;
		QPointer<QLabel> droppedFrames; /* may live outside the block (Lost Frames) */
		QPointer<QLabel> megabytesSent;
		QPointer<QLabel> bitrate;

		bool rec = false;

		vstats::BitrateTracker bitrateTracker;
		vstats::OutputBaseline droppedBaseline;
		long double kbps = 0.0l;

		void Update(obs_output_t *output);
		void Reset(obs_output_t *output);
	};

	QLabel *AddStatRow(QGridLayout *grid, int &row, const QString &name, QLabel **nameOut = nullptr);
	QLabel *AddSectionTitle(const QString &text, QVBoxLayout *into);
	OutputBlock *AddOutputBlock(const QString &title, bool rec, bool ownDroppedRow, QVBoxLayout *into);

	void UpdateGeneral();
	void UpdateOutputs();
	void UpdateVerticalOutputs();
	bool AitumVerticalPresent();
	std::string CurrentOutputPath();

	void StartRecTimeLeft();
	void ResetRecTimeLeft();
	void OnFrontendReady();

	static void OBSFrontendEvent(enum obs_frontend_event event, void *ptr);

	/* general */
	QLabel *cpuUsage = nullptr;
	QLabel *memUsage = nullptr;
	QLabel *hddSpace = nullptr;
	QLabel *hddSpaceName = nullptr; /* hidden with hddSpace when no record folder */
	QLabel *recordTimeLeft = nullptr;
	QLabel *recordTimeLeftName = nullptr;
	QLabel *fps = nullptr;
	QLabel *renderTime = nullptr;

	/* lost frames */
	QLabel *lostRender = nullptr;   /* frames missed due to rendering lag */
	QLabel *lostEncoding = nullptr; /* frames skipped due to encoding lag */
	QLabel *lostNetwork = nullptr;  /* main stream output dropped frames */

	/* outputs */
	OutputBlock *streamBlock = nullptr;
	OutputBlock *recordBlock = nullptr;

	/* Aitum Vertical outputs, keyed by libobs output name */
	QWidget *verticalSection = nullptr;
	QVBoxLayout *verticalLayout = nullptr;
	QMap<QString, OutputBlock *> verticalStreamBlocks;
	OutputBlock *verticalRecordBlock = nullptr;
	bool aitumPresent = false;

	os_cpu_usage_info_t *cpu_info = nullptr;
	QTimer timer;
	QTimer recTimeLeft;
	uint64_t num_bytes = 0;
	std::vector<long double> bitrates;

	vstats::FrameBaseline encodeBaseline; /* encoded / skipped */
	vstats::FrameBaseline renderBaseline; /* rendered / lagged */

	/* The frontend cannot answer output queries until it has finished
	 * loading; querying earlier crashes OBS. Nothing runs before this. */
	bool ready = false;
	bool shuttingDown = false;
};
