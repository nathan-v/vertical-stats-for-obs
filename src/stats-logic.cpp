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

#include "stats-logic.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <numeric>

namespace vstats {

static constexpr uint64_t kMiB = 1024ULL * 1024ULL;
static constexpr uint64_t kGiB = kMiB * 1024ULL;
static constexpr uint64_t kTiB = kGiB * 1024ULL;

/* ---- colouring ---------------------------------------------------------- */

const char *ToneClass(Tone tone)
{
	switch (tone) {
	case Tone::Success:
		return "text-success";
	case Tone::Warning:
		return "text-warning";
	case Tone::Danger:
		return "text-danger";
	case Tone::None:
		break;
	}
	return "";
}

const char *ToneThemeID(Tone tone)
{
	switch (tone) {
	case Tone::Success:
		return "good";
	case Tone::Warning:
		return "warning";
	case Tone::Danger:
		return "error";
	case Tone::None:
		break;
	}
	return "";
}

Tone LostTone(long double pct)
{
	if (pct > 5.0l)
		return Tone::Danger;
	if (pct > 1.0l)
		return Tone::Warning;
	return Tone::None;
}

Tone FpsTone(double currentFps, double targetFps)
{
	if (currentFps < targetFps * 0.8)
		return Tone::Danger;
	if (currentFps < targetFps * 0.95)
		return Tone::Warning;
	return Tone::None;
}

Tone RenderTimeTone(long double renderMs, long double frameBudgetMs)
{
	if (renderMs > frameBudgetMs)
		return Tone::Danger;
	if (renderMs > frameBudgetMs * 0.75l)
		return Tone::Warning;
	return Tone::None;
}

Tone DiskTone(uint64_t freeBytes)
{
	if (freeBytes < kGiB)
		return Tone::Danger;
	if (freeBytes < 5 * kGiB)
		return Tone::Warning;
	return Tone::None;
}

/* ---- video info -------------------------------------------------------- */

double TargetFps(uint32_t fpsNum, uint32_t fpsDen)
{
	return fpsDen ? (double)fpsNum / (double)fpsDen : 0.0;
}

long double FrameBudgetMs(uint32_t fpsNum, uint32_t fpsDen)
{
	return fpsNum ? (long double)fpsDen * 1000.0l / (long double)fpsNum : 0.0l;
}

/* ---- sizes and rates --------------------------------------------------- */

Scaled ScaleDiskFree(uint64_t bytes)
{
	long double num = (long double)bytes / (long double)kMiB;
	if (bytes > kTiB)
		return {num / (1024.0l * 1024.0l), "TB"};
	if (bytes > kGiB)
		return {num / 1024.0l, "GB"};
	return {num, "MB"};
}

Scaled ScaleBytesSent(uint64_t bytes)
{
	long double num = (long double)bytes / (long double)kMiB;
	if (num > 1024)
		return {num / 1024, "GiB"};
	return {num, "MiB"};
}

Scaled ScaleBitrate(long double kbps)
{
	if (kbps >= 10000)
		return {kbps / 1000, "Mb/s"};
	return {kbps, "kb/s"};
}

/* ---- counters ---------------------------------------------------------- */

static long double Percent(uint64_t part, uint64_t total)
{
	return total ? (long double)part / (long double)total * 100.0l : 0.0l;
}

Ratio FrameBaseline::Apply(uint32_t part, uint32_t total)
{
	if (total < firstTotal || part < firstPart) {
		firstTotal = total;
		firstPart = part;
	}
	total -= firstTotal;
	part -= firstPart;
	return {part, total, Percent(part, total)};
}

void FrameBaseline::Reset()
{
	firstTotal = 0xFFFFFFFF;
	firstPart = 0xFFFFFFFF;
}

Ratio OutputBaseline::Apply(int dropped, int total)
{
	if (total < firstTotal || dropped < firstDropped) {
		firstTotal = 0;
		firstDropped = 0;
	}
	total -= firstTotal;
	dropped -= firstDropped;
	return {(uint32_t)dropped, (uint32_t)total, Percent((uint64_t)dropped, (uint64_t)total)};
}

void OutputBaseline::Set(int total, int dropped)
{
	firstTotal = total;
	firstDropped = dropped;
}

long double BitrateTracker::Sample(uint64_t totalBytes, uint64_t nowNs)
{
	uint64_t bytes = totalBytes;
	if (bytes < lastBytes)
		bytes = 0;
	if (bytes == 0)
		lastBytes = 0;

	long double kbps = 0.0l;
	long double secs = (long double)(nowNs - lastTimeNs) / 1000000000.0l;
	if (secs >= 0.01l) {
		long double bits = (long double)(bytes - lastBytes) * 8.0l;
		kbps = bits / secs / 1000.0l;
	}

	lastBytes = bytes;
	lastTimeNs = nowNs;
	return kbps;
}

/* ---- output status ----------------------------------------------------- */

OutputStatus StatusFor(bool isRecording, bool active, bool reconnecting)
{
	if (!active)
		return {OutputState::Inactive, Tone::None};
	if (isRecording)
		return {OutputState::Recording, Tone::None};
	if (reconnecting)
		return {OutputState::Reconnecting, Tone::Danger};
	return {OutputState::Live, Tone::Success};
}

/* ---- disk-full estimate ------------------------------------------------ */

std::optional<TimeLeft> EstimateTimeLeft(const std::vector<long double> &kbpsSamples, uint64_t freeBytes)
{
	if (kbpsSamples.empty())
		return std::nullopt;

	long double average =
		std::accumulate(kbpsSamples.begin(), kbpsSamples.end(), 0.0l) / (long double)kbpsSamples.size();
	if (average == 0)
		return std::nullopt;

	long double bytesPerSec = (average / 8.0l) * 1000.0l;
	long double seconds = (long double)freeBytes / bytesPerSec;

	int totalMinutes = (int)seconds / 60;
	return TimeLeft{totalMinutes / 60, totalMinutes % 60};
}

/* ---- Aitum Vertical ---------------------------------------------------- */

const char *const kAitumStreamPrefix = "vertical_canvas_stream";
const char *const kAitumRecordName = "vertical_canvas_record";
const char *const kAitumProbeProc = "aitum_vertical_get_video";

bool IsAitumStreamOutput(const char *outputName)
{
	return outputName && strncmp(outputName, kAitumStreamPrefix, strlen(kAitumStreamPrefix)) == 0;
}

std::string AitumStreamSuffix(const std::string &outputName)
{
	std::string suffix = outputName.substr(std::min(outputName.size(), strlen(kAitumStreamPrefix)));
	if (!suffix.empty() && suffix[0] == '_')
		suffix.erase(0, 1);
	return suffix;
}

std::string AitumStreamTitle(const std::string &baseTitle, const std::string &outputName)
{
	std::string suffix = AitumStreamSuffix(outputName);
	if (suffix.empty())
		return baseTitle;
	return baseTitle + " (" + suffix + ")";
}

/* ---- recording path ---------------------------------------------------- */

std::string SelectRecordingPath(const char *outputMode, const char *advRecType, const char *advFFmpegPath,
				const char *advRecPath, const char *simplePath)
{
	const char *path = nullptr;
	if (outputMode && strcmp(outputMode, "Advanced") == 0) {
		if (advRecType && strcmp(advRecType, "FFmpeg") == 0)
			path = advFFmpegPath;
		else
			path = advRecPath;
	} else {
		path = simplePath;
	}
	return path ? path : "";
}

/* ---- text -------------------------------------------------------------- */

std::string RatioText(const Ratio &ratio)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%u / %u (%.1Lf%%)", ratio.part, ratio.total, ratio.pct);
	return buf;
}

} // namespace vstats
