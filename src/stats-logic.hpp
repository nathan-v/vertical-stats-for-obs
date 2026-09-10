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

/*
 * Everything the dock computes, with no Qt and no libobs. The widget in
 * vertical-stats.cpp feeds raw counters in and paints the results; this file
 * is what the unit tests in tests/ exercise.
 */

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vstats {

/* ---- colouring ---------------------------------------------------------- */

/* Which theme colour a value gets. Maps onto OBS's text-* classes. */
enum class Tone { None, Success, Warning, Danger };

/* OBS 31+ themes colour by the "class" property. */
const char *ToneClass(Tone tone);
/* OBS 30 and earlier colour by the "themeID" property. */
const char *ToneThemeID(Tone tone);

/* Lost-frame percentages: over 5% is danger, over 1% is warning. */
Tone LostTone(long double pct);
/* Active FPS against the configured FPS: under 80% is danger, under 95% warning. */
Tone FpsTone(double currentFps, double targetFps);
/* Average render time against the frame budget: over budget is danger, over 75% warning. */
Tone RenderTimeTone(long double renderMs, long double frameBudgetMs);
/* Free disk space: under 1 GiB is danger, under 5 GiB warning. */
Tone DiskTone(uint64_t freeBytes);

/* ---- video info -------------------------------------------------------- */

/* Configured FPS from the fps_num/fps_den pair; 0 when the pair is unset. */
double TargetFps(uint32_t fpsNum, uint32_t fpsDen);
/* Milliseconds available per frame; 0 when the pair is unset. */
long double FrameBudgetMs(uint32_t fpsNum, uint32_t fpsDen);

/* ---- sizes and rates --------------------------------------------------- */

struct Scaled {
	long double value;
	const char *unit;
};

/* Free disk space in MB, GB or TB (binary multiples, labelled the way OBS does). */
Scaled ScaleDiskFree(uint64_t bytes);
/* Bytes sent by an output, in MiB or GiB. */
Scaled ScaleBytesSent(uint64_t bytes);
/* Output bitrate in kb/s, or Mb/s once it reaches 10000 kb/s. */
Scaled ScaleBitrate(long double kbps);

/* ---- counters ---------------------------------------------------------- */

struct Ratio {
	uint32_t part;
	uint32_t total;
	long double pct; /* 0 when total is 0 */
};

/* Rebased pair of libobs frame counters (encoded/skipped, rendered/lagged).
 * The first sample, or any sample where a counter went backwards, becomes
 * the new zero point so Reset works and counter restarts do not go negative. */
struct FrameBaseline {
	uint32_t firstTotal = 0xFFFFFFFF;
	uint32_t firstPart = 0xFFFFFFFF;

	Ratio Apply(uint32_t part, uint32_t total);
	void Reset();
};

/* Rebased pair of output frame counters (total/dropped). Matches the
 * built-in panel: a counter going backwards drops the baseline to zero
 * rather than to the current value. */
struct OutputBaseline {
	int firstTotal = 0;
	int firstDropped = 0;

	Ratio Apply(int dropped, int total);
	void Set(int total, int dropped);
};

/* Bitrate between successive samples of an output's total byte count. */
struct BitrateTracker {
	uint64_t lastBytes = 0;
	uint64_t lastTimeNs = 0;

	/* Returns kb/s since the previous sample and records this one. A byte
	 * count that went backwards (output restarted) reads as zero. Samples
	 * closer than 10 ms apart read as zero. */
	long double Sample(uint64_t totalBytes, uint64_t nowNs);
};

/* ---- output status ----------------------------------------------------- */

enum class OutputState { Inactive, Recording, Live, Reconnecting };

struct OutputStatus {
	OutputState state;
	Tone tone;
};

OutputStatus StatusFor(bool isRecording, bool active, bool reconnecting);

/* ---- disk-full estimate ------------------------------------------------ */

struct TimeLeft {
	int hours;
	int minutes;
};

/* Hours and minutes until the recording folder fills, from the recording
 * bitrates sampled since the last estimate. Empty when there is nothing to
 * average or the average is zero, in which case the previous text stands. */
std::optional<TimeLeft> EstimateTimeLeft(const std::vector<long double> &kbpsSamples, uint64_t freeBytes);

/* ---- Aitum Vertical ---------------------------------------------------- */

extern const char *const kAitumStreamPrefix; /* "vertical_canvas_stream" */
extern const char *const kAitumRecordName;   /* "vertical_canvas_record" */
extern const char *const kAitumProbeProc;    /* "aitum_vertical_get_video" */

/* True for "vertical_canvas_stream" and "vertical_canvas_stream_<server>". */
bool IsAitumStreamOutput(const char *outputName);
/* The <server> part of an Aitum stream output name, or "" for the unnamed one. */
std::string AitumStreamSuffix(const std::string &outputName);
/* Block title: baseTitle, or "baseTitle (<server>)" when there is a suffix. */
std::string AitumStreamTitle(const std::string &baseTitle, const std::string &outputName);

/* ---- recording path ---------------------------------------------------- */

/* Mirrors OBSBasic::GetCurrentOutputPath. Any argument may be null. */
std::string SelectRecordingPath(const char *outputMode, const char *advRecType, const char *advFFmpegPath,
				const char *advRecPath, const char *simplePath);

/* ---- text -------------------------------------------------------------- */

/* "part / total (pct%)" with one decimal on the percentage. */
std::string RatioText(const Ratio &ratio);

} // namespace vstats
