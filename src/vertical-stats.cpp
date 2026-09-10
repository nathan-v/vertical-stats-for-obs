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

#include "vertical-stats.hpp"

#include <obs-module.h>
#include <util/config-file.h>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStyle>
#include <QVBoxLayout>

using namespace vstats;

#define TIMER_INTERVAL 2000
#define REC_TIME_LEFT_INTERVAL 30000

/* ------------------------------------------------------------------------- */
/* helpers                                                                   */

/* OBS's own translated strings (used for status words like Live / Recording). */
static QString FrontendStr(const char *key)
{
	const char *s = obs_frontend_get_locale_string(key);
	return QString::fromUtf8(s && *s ? s : key);
}

static QString PluginStr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/* OBS themes colour labels through the "class" property (31+) and the older
 * "themeID" property (30 and earlier). Set both so any theme picks it up. */
static void SetTone(QWidget *widget, Tone tone)
{
	widget->setProperty("class", ToneClass(tone));
	widget->setProperty("themeID", ToneThemeID(tone));
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

static QString MakeTimeLeftText(int hours, int minutes)
{
	return PluginStr("VerticalStats.DiskFullIn.Text").arg(QString::number(hours), QString::number(minutes));
}

static QString ScaledText(const Scaled &s, int decimals)
{
	return QString::number(s.value, 'f', decimals) + QStringLiteral(" ") + QString::fromUtf8(s.unit);
}

static void SetRatio(QLabel *label, const Ratio &ratio)
{
	label->setText(QString::fromStdString(RatioText(ratio)));
	SetTone(label, LostTone(ratio.pct));
}

static QLabel *ValueLabel(QWidget *parent)
{
	QLabel *l = new QLabel(parent);
	l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	l->setTextInteractionFlags(Qt::TextSelectableByMouse);
	return l;
}

/* ------------------------------------------------------------------------- */
/* construction                                                              */

QLabel *VerticalStats::AddStatRow(QGridLayout *grid, int &row, const QString &name, QLabel **nameOut)
{
	QLabel *nameLabel = new QLabel(name, this);
	QLabel *value = ValueLabel(this);
	grid->addWidget(nameLabel, row, 0);
	grid->addWidget(value, row, 1);
	row++;
	if (nameOut)
		*nameOut = nameLabel;
	return value;
}

QLabel *VerticalStats::AddSectionTitle(const QString &text, QVBoxLayout *into)
{
	QLabel *title = new QLabel(text, this);
	title->setStyleSheet("font-weight: bold");
	into->addWidget(title);
	return title;
}

VerticalStats::OutputBlock *VerticalStats::AddOutputBlock(const QString &title, bool rec, bool ownDroppedRow,
							  QVBoxLayout *into)
{
	OutputBlock *ob = new OutputBlock();
	ob->rec = rec;

	QFrame *frame = new QFrame(this);
	frame->setFrameShape(QFrame::StyledPanel);
	QVBoxLayout *v = new QVBoxLayout(frame);
	v->setContentsMargins(6, 4, 6, 4);
	v->setSpacing(2);

	QHBoxLayout *head = new QHBoxLayout();
	ob->title = new QLabel(title, frame);
	ob->title->setStyleSheet("font-weight: bold");
	ob->status = ValueLabel(frame);
	head->addWidget(ob->title);
	head->addStretch();
	head->addWidget(ob->status);
	v->addLayout(head);

	QGridLayout *grid = new QGridLayout();
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setColumnStretch(1, 1);
	int row = 0;

	ob->megabytesSent = AddStatRow(grid, row, PluginStr("VerticalStats.Sent"));
	ob->bitrate = AddStatRow(grid, row, PluginStr("VerticalStats.Bitrate"));
	if (!rec && ownDroppedRow)
		ob->droppedFrames = AddStatRow(grid, row, PluginStr("VerticalStats.Lost.Network"));
	v->addLayout(grid);

	ob->frame = frame;
	into->addWidget(frame);
	return ob;
}

VerticalStats::VerticalStats(QWidget *parent)
	: QFrame(parent),
	  cpu_info(os_cpu_usage_info_start()),
	  timer(this),
	  recTimeLeft(this)
{
	bitrates.reserve(REC_TIME_LEFT_INTERVAL / TIMER_INTERVAL);

	QWidget *content = new QWidget(this);
	QVBoxLayout *layout = new QVBoxLayout(content);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(8);

	/* ---- general (no heading) -------------------------------------- */
	QGridLayout *grid = new QGridLayout();
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setColumnStretch(1, 1);
	int row = 0;
	cpuUsage = AddStatRow(grid, row, PluginStr("VerticalStats.CPU"));
	fps = AddStatRow(grid, row, QStringLiteral("FPS"));
	renderTime = AddStatRow(grid, row, PluginStr("VerticalStats.RenderTime"));
	memUsage = AddStatRow(grid, row, PluginStr("VerticalStats.Memory"));
	hddSpace = AddStatRow(grid, row, PluginStr("VerticalStats.DiskFree"), &hddSpaceName);
	recordTimeLeft = AddStatRow(grid, row, PluginStr("VerticalStats.DiskFullIn"), &recordTimeLeftName);
	layout->addLayout(grid);

	/* ---- lost frames ----------------------------------------------- */
	QLabel *lostTitle = AddSectionTitle(PluginStr("VerticalStats.Lost"), layout);
	lostTitle->setAlignment(Qt::AlignHCenter);

	grid = new QGridLayout();
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setColumnStretch(1, 1);
	row = 0;
	lostRender = AddStatRow(grid, row, PluginStr("VerticalStats.Lost.Render"));
	lostEncoding = AddStatRow(grid, row, PluginStr("VerticalStats.Lost.Encoding"));
	lostNetwork = AddStatRow(grid, row, PluginStr("VerticalStats.Lost.Network"));
	layout->addLayout(grid);
	layout->addSpacing(10);

	/* ---- main outputs ---------------------------------------------- */
	streamBlock = AddOutputBlock(FrontendStr("Basic.Stats.Output.Stream"), false, false, layout);
	streamBlock->droppedFrames = lostNetwork; /* shown under Lost Frames instead */
	recordBlock = AddOutputBlock(FrontendStr("Basic.Stats.Output.Recording"), true, false, layout);

	/* ---- Aitum Vertical outputs (hidden until the plugin is detected) */
	verticalSection = new QWidget(content);
	verticalLayout = new QVBoxLayout(verticalSection);
	verticalLayout->setContentsMargins(0, 0, 0, 0);
	verticalLayout->setSpacing(8);
	verticalRecordBlock =
		AddOutputBlock(PluginStr("VerticalStats.Output.VerticalRecording"), true, false, verticalLayout);
	verticalSection->hide();
	layout->addWidget(verticalSection);

	/* ---- reset ----------------------------------------------------- */
	QPushButton *resetButton = new QPushButton(FrontendStr("Reset"), content);
	connect(resetButton, &QPushButton::clicked, this, &VerticalStats::Reset);
	layout->addWidget(resetButton);
	layout->addStretch();

	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidget(content);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);

	QVBoxLayout *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->addWidget(scroll);

	setMinimumWidth(180);

	connect(&timer, &QTimer::timeout, this, &VerticalStats::Update);
	timer.setInterval(TIMER_INTERVAL);

	connect(&recTimeLeft, &QTimer::timeout, this, &VerticalStats::RecordingTimeLeft);
	recTimeLeft.setInterval(REC_TIME_LEFT_INTERVAL);

	/* No libobs/frontend queries here: we are constructed inside
	 * obs_module_load, before the frontend can answer. See OnFrontendReady. */
	obs_frontend_add_event_callback(OBSFrontendEvent, this);
}

VerticalStats::~VerticalStats()
{
	obs_frontend_remove_event_callback(OBSFrontendEvent, this);
	os_cpu_usage_info_destroy(cpu_info);
	for (OutputBlock *ob : verticalStreamBlocks)
		delete ob;
	delete verticalRecordBlock;
	delete streamBlock;
	delete recordBlock;
}

void VerticalStats::OnFrontendReady()
{
	ready = true;
	if (obs_frontend_recording_active())
		StartRecTimeLeft();
	Update();
	if (isVisible())
		timer.start(TIMER_INTERVAL);
}

void VerticalStats::OBSFrontendEvent(enum obs_frontend_event event, void *ptr)
{
	VerticalStats *stats = static_cast<VerticalStats *>(ptr);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		stats->OnFrontendReady();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		stats->StartRecTimeLeft();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		stats->ResetRecTimeLeft();
		break;
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
	case OBS_FRONTEND_EVENT_EXIT:
		/* Stop touching libobs while it tears down. */
		stats->shuttingDown = true;
		stats->timer.stop();
		stats->recTimeLeft.stop();
		break;
	default:
		break;
	}
}

void VerticalStats::showEvent(QShowEvent *)
{
	if (ready && !shuttingDown)
		timer.start(TIMER_INTERVAL);
}

void VerticalStats::hideEvent(QHideEvent *)
{
	timer.stop();
}

/* ------------------------------------------------------------------------- */
/* recording path (mirrors OBSBasic::GetCurrentOutputPath)                   */

std::string VerticalStats::CurrentOutputPath()
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config)
		return "";

	return SelectRecordingPath(config_get_string(config, "Output", "Mode"),
				   config_get_string(config, "AdvOut", "RecType"),
				   config_get_string(config, "AdvOut", "FFFilePath"),
				   config_get_string(config, "AdvOut", "RecFilePath"),
				   config_get_string(config, "SimpleOutput", "FilePath"));
}

/* ------------------------------------------------------------------------- */
/* Aitum Vertical detection                                                  */

bool VerticalStats::AitumVerticalPresent()
{
	if (aitumPresent)
		return true;

	/* Aitum registers its proc handlers at module load. Our module may load
	 * first, so keep probing on each tick until it shows up. The probe proc
	 * only reads a pointer; it has no side effects. */
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_int(&cd, "width", 0);
	calldata_set_int(&cd, "height", 0);
	bool found = proc_handler_call(obs_get_proc_handler(), kAitumProbeProc, &cd);
	calldata_free(&cd);

	if (found) {
		aitumPresent = true;
		verticalSection->show();
		blog(LOG_INFO, "[vertical-stats] Aitum Vertical detected, showing vertical output stats");
	}
	return aitumPresent;
}

/* ------------------------------------------------------------------------- */
/* update                                                                    */

void VerticalStats::Update()
{
	if (!ready || shuttingDown)
		return;

	UpdateGeneral();
	UpdateOutputs();
	if (AitumVerticalPresent())
		UpdateVerticalOutputs();
}

void VerticalStats::UpdateGeneral()
{
	struct obs_video_info ovi = {};
	obs_get_video_info(&ovi);

	/* FPS */
	double curFPS = obs_get_active_fps();
	fps->setText(QString::number(curFPS, 'f', 2));
	SetTone(fps, FpsTone(curFPS, TargetFps(ovi.fps_num, ovi.fps_den)));

	/* CPU */
	double usage = os_cpu_usage_info_query(cpu_info);
	cpuUsage->setText(QString::number(usage, 'g', 2) + QStringLiteral("%"));

	/* Disk */
	std::string path = CurrentOutputPath();
	num_bytes = path.empty() ? 0 : os_get_free_disk_space(path.c_str());

	/* No recording folder set (or it doesn't exist): the disk rows would only
	 * ever show a red 0.0 MB, so hide them until a folder is configured. */
	bool haveDisk = num_bytes > 0;
	hddSpaceName->setVisible(haveDisk);
	hddSpace->setVisible(haveDisk);
	recordTimeLeftName->setVisible(haveDisk);
	recordTimeLeft->setVisible(haveDisk);

	hddSpace->setText(ScaledText(ScaleDiskFree(num_bytes), 1));
	SetTone(hddSpace, DiskTone(num_bytes));

	/* Memory */
	long double num = (long double)os_get_proc_resident_size() / (1024.0l * 1024.0l);
	memUsage->setText(QString::number(num, 'f', 1) + QStringLiteral(" MB"));

	/* Render time */
	num = (long double)obs_get_average_frame_time_ns() / 1000000.0l;
	renderTime->setText(QString::number(num, 'f', 1) + QStringLiteral(" ms"));
	SetTone(renderTime, RenderTimeTone(num, FrameBudgetMs(ovi.fps_num, ovi.fps_den)));

	/* Lost: Encoding (frames skipped due to encoding lag) */
	video_t *video = obs_get_video();
	SetRatio(lostEncoding,
		 encodeBaseline.Apply(video_output_get_skipped_frames(video), video_output_get_total_frames(video)));

	/* Lost: Render (frames missed due to rendering lag) */
	SetRatio(lostRender, renderBaseline.Apply(obs_get_lagged_frames(), obs_get_total_frames()));
}

void VerticalStats::UpdateOutputs()
{
	obs_output_t *strOutput = obs_frontend_get_streaming_output();
	obs_output_t *recOutput = obs_frontend_get_recording_output();

	streamBlock->Update(strOutput); /* also fills Lost Frames -> Network */
	recordBlock->Update(recOutput);

	if (recOutput && obs_output_active(recOutput))
		bitrates.push_back(recordBlock->kbps);

	obs_output_release(strOutput);
	obs_output_release(recOutput);
}

/* Collect Aitum Vertical stream outputs by name. Called on the UI thread. */
struct EnumCtx {
	std::vector<obs_output_t *> streams;
};

static bool EnumAitumOutputs(void *param, obs_output_t *output)
{
	EnumCtx *ctx = static_cast<EnumCtx *>(param);
	if (IsAitumStreamOutput(obs_output_get_name(output))) {
		obs_output_t *ref = obs_output_get_ref(output);
		if (ref)
			ctx->streams.push_back(ref);
	}
	return true;
}

void VerticalStats::UpdateVerticalOutputs()
{
	/* Stream outputs: Aitum names them "vertical_canvas_stream" or
	 * "vertical_canvas_stream_<server name>", one per configured server. */
	EnumCtx ctx;
	obs_enum_outputs(EnumAitumOutputs, &ctx);

	QSet<QString> seen;
	for (obs_output_t *output : ctx.streams) {
		QString name = QString::fromUtf8(obs_output_get_name(output));
		seen.insert(name);

		OutputBlock *ob = verticalStreamBlocks.value(name, nullptr);
		if (!ob) {
			std::string base = PluginStr("VerticalStats.Output.VerticalStream").toStdString();
			QString title = QString::fromStdString(AitumStreamTitle(base, name.toStdString()));

			ob = AddOutputBlock(title, false, true, verticalLayout);
			/* keep the recording block last */
			verticalLayout->removeWidget(verticalRecordBlock->frame);
			verticalLayout->addWidget(verticalRecordBlock->frame);
			verticalStreamBlocks.insert(name, ob);
		}
		ob->Update(output);
		obs_output_release(output);
	}

	/* Outputs that disappeared (server removed in Aitum) go inactive rather
	 * than vanishing, so the layout doesn't jump mid-stream. */
	for (auto it = verticalStreamBlocks.begin(); it != verticalStreamBlocks.end(); ++it) {
		if (!seen.contains(it.key()))
			it.value()->Update(nullptr);
	}

	/* Recording output: created by Aitum when its recording starts. */
	obs_output_t *rec = obs_get_output_by_name(kAitumRecordName);
	verticalRecordBlock->Update(rec);
	obs_output_release(rec);
}

/* ------------------------------------------------------------------------- */
/* disk-full estimate                                                        */

void VerticalStats::StartRecTimeLeft()
{
	if (recTimeLeft.isActive())
		ResetRecTimeLeft();

	recordTimeLeft->setText(FrontendStr("Calculating"));
	recTimeLeft.start();
}

void VerticalStats::ResetRecTimeLeft()
{
	if (recTimeLeft.isActive()) {
		bitrates.clear();
		recTimeLeft.stop();
		recordTimeLeft->setText(QString());
	}
}

void VerticalStats::RecordingTimeLeft()
{
	if (shuttingDown)
		return;

	std::optional<TimeLeft> left = EstimateTimeLeft(bitrates, num_bytes);
	if (!left)
		return; /* nothing usable yet; keep the previous text */

	bitrates.clear();
	recordTimeLeft->setText(MakeTimeLeftText(left->hours, left->minutes));
}

/* ------------------------------------------------------------------------- */
/* reset                                                                     */

void VerticalStats::Reset()
{
	if (!ready || shuttingDown)
		return;

	timer.start();

	encodeBaseline.Reset();
	renderBaseline.Reset();

	obs_output_t *strOutput = obs_frontend_get_streaming_output();
	obs_output_t *recOutput = obs_frontend_get_recording_output();
	streamBlock->Reset(strOutput);
	recordBlock->Reset(recOutput);
	obs_output_release(strOutput);
	obs_output_release(recOutput);

	if (aitumPresent) {
		EnumCtx ctx;
		obs_enum_outputs(EnumAitumOutputs, &ctx);
		for (obs_output_t *output : ctx.streams) {
			QString name = QString::fromUtf8(obs_output_get_name(output));
			OutputBlock *ob = verticalStreamBlocks.value(name, nullptr);
			if (ob)
				ob->Reset(output);
			obs_output_release(output);
		}
		obs_output_t *rec = obs_get_output_by_name(kAitumRecordName);
		verticalRecordBlock->Reset(rec);
		obs_output_release(rec);
	}

	Update();
}

/* ------------------------------------------------------------------------- */
/* per-output block                                                          */

static const char *StatusKey(OutputState state)
{
	switch (state) {
	case OutputState::Recording:
		return "Basic.Stats.Status.Recording";
	case OutputState::Live:
		return "Basic.Stats.Status.Live";
	case OutputState::Reconnecting:
		return "Basic.Stats.Status.Reconnecting";
	case OutputState::Inactive:
		break;
	}
	return "Basic.Stats.Status.Inactive";
}

void VerticalStats::OutputBlock::Update(obs_output_t *output)
{
	uint64_t totalBytes = output ? obs_output_get_total_bytes(output) : 0;
	kbps = bitrateTracker.Sample(totalBytes, os_gettime_ns());

	bool active = output ? obs_output_active(output) : false;
	bool reconnecting = active && !rec ? obs_output_reconnecting(output) : false;
	OutputStatus st = StatusFor(rec, active, reconnecting);
	status->setText(FrontendStr(StatusKey(st.state)));
	SetTone(status, st.tone);

	megabytesSent->setText(ScaledText(ScaleBytesSent(totalBytes), 1));
	bitrate->setText(ScaledText(ScaleBitrate(kbps), 0));

	if (!rec && droppedFrames) {
		int total = output ? obs_output_get_total_frames(output) : 0;
		int dropped = output ? obs_output_get_frames_dropped(output) : 0;
		SetRatio(droppedFrames, droppedBaseline.Apply(dropped, total));
	}
}

void VerticalStats::OutputBlock::Reset(obs_output_t *output)
{
	if (!output)
		return;

	droppedBaseline.Set(obs_output_get_total_frames(output), obs_output_get_frames_dropped(output));
}
