/*
Vertical Stats for OBS
Copyright (C) 2026 Nathan V

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

#include "check.hpp"
#include "stats-logic.hpp"

using namespace vstats;

static constexpr uint64_t MiB = 1024ULL * 1024ULL;
static constexpr uint64_t GiB = MiB * 1024ULL;
static constexpr uint64_t TiB = GiB * 1024ULL;

static int T(Tone tone)
{
	return (int)tone;
}

static int S(OutputState state)
{
	return (int)state;
}

/* ---- colouring ---------------------------------------------------------- */

TEST_CASE(tone_maps_to_obs_class_and_theme_id)
{
	CHECK_STR(ToneClass(Tone::None), "");
	CHECK_STR(ToneClass(Tone::Success), "text-success");
	CHECK_STR(ToneClass(Tone::Warning), "text-warning");
	CHECK_STR(ToneClass(Tone::Danger), "text-danger");

	CHECK_STR(ToneThemeID(Tone::None), "");
	CHECK_STR(ToneThemeID(Tone::Success), "good");
	CHECK_STR(ToneThemeID(Tone::Warning), "warning");
	CHECK_STR(ToneThemeID(Tone::Danger), "error");
}

TEST_CASE(lost_frame_thresholds_are_one_and_five_percent)
{
	CHECK_EQ(T(LostTone(0.0l)), T(Tone::None));
	CHECK_EQ(T(LostTone(1.0l)), T(Tone::None));
	CHECK_EQ(T(LostTone(1.01l)), T(Tone::Warning));
	CHECK_EQ(T(LostTone(5.0l)), T(Tone::Warning));
	CHECK_EQ(T(LostTone(5.01l)), T(Tone::Danger));
	CHECK_EQ(T(LostTone(100.0l)), T(Tone::Danger));
}

TEST_CASE(fps_thresholds_are_80_and_95_percent_of_target)
{
	CHECK_EQ(T(FpsTone(60.0, 60.0)), T(Tone::None));
	CHECK_EQ(T(FpsTone(57.0, 60.0)), T(Tone::None)); /* exactly 95% is fine */
	CHECK_EQ(T(FpsTone(56.9, 60.0)), T(Tone::Warning));
	CHECK_EQ(T(FpsTone(48.0, 60.0)), T(Tone::Warning)); /* exactly 80% is a warning */
	CHECK_EQ(T(FpsTone(47.9, 60.0)), T(Tone::Danger));
	CHECK_EQ(T(FpsTone(0.0, 60.0)), T(Tone::Danger));
	/* No video configured: nothing to compare against, no colour. */
	CHECK_EQ(T(FpsTone(0.0, 0.0)), T(Tone::None));
}

TEST_CASE(render_time_thresholds_are_75_and_100_percent_of_budget)
{
	const long double budget = 1000.0l / 60.0l;
	CHECK_EQ(T(RenderTimeTone(5.0l, budget)), T(Tone::None));
	CHECK_EQ(T(RenderTimeTone(budget * 0.75l, budget)), T(Tone::None));
	CHECK_EQ(T(RenderTimeTone(budget * 0.76l, budget)), T(Tone::Warning));
	CHECK_EQ(T(RenderTimeTone(budget, budget)), T(Tone::Warning));
	CHECK_EQ(T(RenderTimeTone(budget + 0.1l, budget)), T(Tone::Danger));
	/* No video: zero budget, so any render time at all shows as over budget. */
	CHECK_EQ(T(RenderTimeTone(0.0l, 0.0l)), T(Tone::None));
	CHECK_EQ(T(RenderTimeTone(0.1l, 0.0l)), T(Tone::Danger));
}

TEST_CASE(disk_thresholds_are_one_and_five_gib)
{
	CHECK_EQ(T(DiskTone(0)), T(Tone::Danger));
	CHECK_EQ(T(DiskTone(GiB - 1)), T(Tone::Danger));
	CHECK_EQ(T(DiskTone(GiB)), T(Tone::Warning));
	CHECK_EQ(T(DiskTone(5 * GiB - 1)), T(Tone::Warning));
	CHECK_EQ(T(DiskTone(5 * GiB)), T(Tone::None));
	CHECK_EQ(T(DiskTone(TiB)), T(Tone::None));
}

/* ---- video info -------------------------------------------------------- */

TEST_CASE(target_fps_and_frame_budget_from_fraction)
{
	CHECK_NEAR(TargetFps(60, 1), 60.0, 1e-9);
	CHECK_NEAR(TargetFps(60000, 1001), 59.94005994, 1e-6);
	CHECK_NEAR(TargetFps(30, 1), 30.0, 1e-9);
	CHECK_EQ(TargetFps(60, 0), 0.0);
	CHECK_EQ(TargetFps(0, 0), 0.0);

	CHECK_NEAR(FrameBudgetMs(60, 1), 16.6666666667l, 1e-6l);
	CHECK_NEAR(FrameBudgetMs(30, 1), 33.3333333333l, 1e-6l);
	CHECK_NEAR(FrameBudgetMs(60000, 1001), 16.6833333333l, 1e-6l);
	CHECK_EQ(FrameBudgetMs(0, 1), 0.0l);
}

/* ---- sizes and rates --------------------------------------------------- */

TEST_CASE(disk_free_scales_mb_gb_tb_with_obs_labels)
{
	Scaled s = ScaleDiskFree(500 * MiB);
	CHECK_NEAR(s.value, 500.0l, 1e-9l);
	CHECK_STR(s.unit, "MB");

	/* Exactly 1 GiB is still shown in MB; the panel only switches above it. */
	s = ScaleDiskFree(GiB);
	CHECK_NEAR(s.value, 1024.0l, 1e-9l);
	CHECK_STR(s.unit, "MB");

	s = ScaleDiskFree(GiB + 1);
	CHECK_NEAR(s.value, 1.0l, 1e-6l);
	CHECK_STR(s.unit, "GB");

	s = ScaleDiskFree(250 * GiB);
	CHECK_NEAR(s.value, 250.0l, 1e-9l);
	CHECK_STR(s.unit, "GB");

	s = ScaleDiskFree(3 * TiB);
	CHECK_NEAR(s.value, 3.0l, 1e-9l);
	CHECK_STR(s.unit, "TB");

	s = ScaleDiskFree(0);
	CHECK_EQ(s.value, 0.0l);
	CHECK_STR(s.unit, "MB");
}

TEST_CASE(bytes_sent_scales_mib_gib)
{
	Scaled s = ScaleBytesSent(10 * MiB);
	CHECK_NEAR(s.value, 10.0l, 1e-9l);
	CHECK_STR(s.unit, "MiB");

	s = ScaleBytesSent(1024 * MiB);
	CHECK_NEAR(s.value, 1024.0l, 1e-9l);
	CHECK_STR(s.unit, "MiB");

	s = ScaleBytesSent(2048 * MiB);
	CHECK_NEAR(s.value, 2.0l, 1e-9l);
	CHECK_STR(s.unit, "GiB");

	s = ScaleBytesSent(0);
	CHECK_EQ(s.value, 0.0l);
	CHECK_STR(s.unit, "MiB");
}

TEST_CASE(bitrate_switches_to_mbps_at_ten_thousand)
{
	Scaled s = ScaleBitrate(0.0l);
	CHECK_EQ(s.value, 0.0l);
	CHECK_STR(s.unit, "kb/s");

	s = ScaleBitrate(6000.0l);
	CHECK_NEAR(s.value, 6000.0l, 1e-9l);
	CHECK_STR(s.unit, "kb/s");

	s = ScaleBitrate(9999.9l);
	CHECK_STR(s.unit, "kb/s");

	s = ScaleBitrate(10000.0l);
	CHECK_NEAR(s.value, 10.0l, 1e-9l);
	CHECK_STR(s.unit, "Mb/s");

	s = ScaleBitrate(25000.0l);
	CHECK_NEAR(s.value, 25.0l, 1e-9l);
	CHECK_STR(s.unit, "Mb/s");
}

/* ---- counters ---------------------------------------------------------- */

TEST_CASE(frame_baseline_zeroes_on_first_sample)
{
	FrameBaseline b;
	Ratio r = b.Apply(5, 100);
	CHECK_EQ(r.part, 0u);
	CHECK_EQ(r.total, 0u);
	CHECK_EQ(r.pct, 0.0l);
	CHECK_EQ(b.firstTotal, 100u);
	CHECK_EQ(b.firstPart, 5u);
}

TEST_CASE(frame_baseline_reports_delta_since_baseline)
{
	FrameBaseline b;
	b.Apply(5, 100);
	Ratio r = b.Apply(7, 200);
	CHECK_EQ(r.part, 2u);
	CHECK_EQ(r.total, 100u);
	CHECK_NEAR(r.pct, 2.0l, 1e-9l);

	r = b.Apply(57, 1100);
	CHECK_EQ(r.part, 52u);
	CHECK_EQ(r.total, 1000u);
	CHECK_NEAR(r.pct, 5.2l, 1e-9l);
}

TEST_CASE(frame_baseline_rebases_when_a_counter_goes_backwards)
{
	FrameBaseline b;
	b.Apply(5, 100);
	b.Apply(7, 200);

	/* Video restarted: totals dropped. New baseline is the current sample. */
	Ratio r = b.Apply(1, 50);
	CHECK_EQ(r.part, 0u);
	CHECK_EQ(r.total, 0u);
	CHECK_EQ(b.firstTotal, 50u);
	CHECK_EQ(b.firstPart, 1u);

	/* Only the part counter going backwards also rebases. */
	b.Apply(3, 150);
	r = b.Apply(0, 160);
	CHECK_EQ(r.part, 0u);
	CHECK_EQ(r.total, 0u);
	CHECK_EQ(b.firstPart, 0u);
	CHECK_EQ(b.firstTotal, 160u);
}

TEST_CASE(frame_baseline_reset_makes_next_sample_the_baseline)
{
	FrameBaseline b;
	b.Apply(5, 100);
	b.Apply(7, 200);
	b.Reset();
	CHECK_EQ(b.firstTotal, 0xFFFFFFFFu);
	CHECK_EQ(b.firstPart, 0xFFFFFFFFu);

	Ratio r = b.Apply(9, 300);
	CHECK_EQ(r.part, 0u);
	CHECK_EQ(r.total, 0u);
	r = b.Apply(10, 350);
	CHECK_EQ(r.part, 1u);
	CHECK_EQ(r.total, 50u);
	CHECK_NEAR(r.pct, 2.0l, 1e-9l);
}

TEST_CASE(frame_baseline_percent_is_zero_when_total_is_zero)
{
	FrameBaseline b;
	b.Apply(0, 0);
	Ratio r = b.Apply(0, 0);
	CHECK_EQ(r.total, 0u);
	CHECK_EQ(r.pct, 0.0l);
}

TEST_CASE(output_baseline_defaults_to_zero)
{
	OutputBaseline b;
	Ratio r = b.Apply(3, 100);
	CHECK_EQ(r.part, 3u);
	CHECK_EQ(r.total, 100u);
	CHECK_NEAR(r.pct, 3.0l, 1e-9l);
}

TEST_CASE(output_baseline_set_from_reset_subtracts)
{
	OutputBaseline b;
	b.Set(100, 3);
	Ratio r = b.Apply(4, 110);
	CHECK_EQ(r.part, 1u);
	CHECK_EQ(r.total, 10u);
	CHECK_NEAR(r.pct, 10.0l, 1e-9l);
}

TEST_CASE(output_baseline_drops_to_zero_when_a_counter_goes_backwards)
{
	OutputBaseline b;
	b.Set(100, 3);

	/* Output restarted: unlike the frame counters, the baseline becomes 0. */
	Ratio r = b.Apply(0, 50);
	CHECK_EQ(r.part, 0u);
	CHECK_EQ(r.total, 50u);
	CHECK_EQ(b.firstTotal, 0);
	CHECK_EQ(b.firstDropped, 0);
}

TEST_CASE(output_baseline_handles_inactive_output_as_zero_counters)
{
	OutputBaseline b;
	Ratio r = b.Apply(0, 0);
	CHECK_EQ(r.part, 0u);
	CHECK_EQ(r.total, 0u);
	CHECK_EQ(r.pct, 0.0l);
}

TEST_CASE(bitrate_tracker_computes_kbps_between_samples)
{
	const uint64_t sec = 1000000000ULL;
	BitrateTracker t;
	t.Sample(0, 10 * sec);

	/* 250,000 bytes in one second is 2,000,000 bits/s, i.e. 2000 kb/s. */
	CHECK_NEAR(t.Sample(250000, 11 * sec), 2000.0l, 1e-6l);
	/* Another 500,000 bytes over two seconds: also 2000 kb/s. */
	CHECK_NEAR(t.Sample(750000, 13 * sec), 2000.0l, 1e-6l);
	/* Nothing new sent. */
	CHECK_EQ(t.Sample(750000, 15 * sec), 0.0l);
	CHECK_EQ(t.lastBytes, 750000u);
	CHECK_EQ(t.lastTimeNs, 15 * sec);
}

TEST_CASE(bitrate_tracker_reads_zero_when_samples_are_too_close)
{
	const uint64_t sec = 1000000000ULL;
	BitrateTracker t;
	t.Sample(0, sec);
	CHECK_EQ(t.Sample(1000000, sec + 5000000), 0.0l); /* 5 ms later */
	CHECK_EQ(t.Sample(2000000, sec + 5000000), 0.0l); /* same instant */
	/* The sample was still recorded. */
	CHECK_EQ(t.lastBytes, 2000000u);
}

TEST_CASE(bitrate_tracker_resets_when_byte_count_goes_backwards)
{
	const uint64_t sec = 1000000000ULL;
	BitrateTracker t;
	t.Sample(0, sec);
	t.Sample(1000000, 2 * sec);

	/* Output restarted with a fresh counter: no negative or huge rate. */
	CHECK_EQ(t.Sample(500, 3 * sec), 0.0l);
	CHECK_EQ(t.lastBytes, 0u);

	/* From that zero point it counts normally again. */
	CHECK_NEAR(t.Sample(125000, 4 * sec), 1000.0l, 1e-6l);
}

TEST_CASE(bitrate_tracker_treats_zero_bytes_as_inactive)
{
	const uint64_t sec = 1000000000ULL;
	BitrateTracker t;
	t.Sample(1000000, sec);
	CHECK_EQ(t.Sample(0, 2 * sec), 0.0l);
	CHECK_EQ(t.lastBytes, 0u);
}

/* ---- output status ----------------------------------------------------- */

TEST_CASE(status_for_recording_output)
{
	OutputStatus s = StatusFor(true, true, false);
	CHECK_EQ(S(s.state), S(OutputState::Recording));
	CHECK_EQ(T(s.tone), T(Tone::None));

	s = StatusFor(true, false, false);
	CHECK_EQ(S(s.state), S(OutputState::Inactive));
	CHECK_EQ(T(s.tone), T(Tone::None));

	/* Reconnecting is meaningless for a recording; it is never reported. */
	s = StatusFor(true, true, true);
	CHECK_EQ(S(s.state), S(OutputState::Recording));
}

TEST_CASE(status_for_stream_output)
{
	OutputStatus s = StatusFor(false, true, false);
	CHECK_EQ(S(s.state), S(OutputState::Live));
	CHECK_EQ(T(s.tone), T(Tone::Success));

	s = StatusFor(false, true, true);
	CHECK_EQ(S(s.state), S(OutputState::Reconnecting));
	CHECK_EQ(T(s.tone), T(Tone::Danger));

	s = StatusFor(false, false, false);
	CHECK_EQ(S(s.state), S(OutputState::Inactive));
	CHECK_EQ(T(s.tone), T(Tone::None));

	/* An inactive output is inactive even if a stale reconnecting flag is set. */
	s = StatusFor(false, false, true);
	CHECK_EQ(S(s.state), S(OutputState::Inactive));
	CHECK_EQ(T(s.tone), T(Tone::None));
}

/* ---- disk-full estimate ------------------------------------------------ */

TEST_CASE(time_left_needs_samples_and_a_nonzero_average)
{
	CHECK(!EstimateTimeLeft({}, GiB).has_value());
	CHECK(!EstimateTimeLeft({0.0l, 0.0l}, GiB).has_value());
}

TEST_CASE(time_left_from_a_single_bitrate)
{
	/* 8000 kb/s is exactly 1,000,000 bytes/s. 5.4 GB fills in 90 minutes. */
	auto t = EstimateTimeLeft({8000.0l}, 5400000000ULL);
	CHECK(t.has_value());
	CHECK_EQ(t->hours, 1);
	CHECK_EQ(t->minutes, 30);
}

TEST_CASE(time_left_averages_the_samples)
{
	auto t = EstimateTimeLeft({4000.0l, 12000.0l}, 5400000000ULL);
	CHECK(t.has_value());
	CHECK_EQ(t->hours, 1);
	CHECK_EQ(t->minutes, 30);
}

TEST_CASE(time_left_rounds_down_to_whole_minutes)
{
	/* 1,000,000 bytes/s and 119,999,999 bytes: 119.99 s, so 1 minute. */
	auto t = EstimateTimeLeft({8000.0l}, 119999999ULL);
	CHECK(t.has_value());
	CHECK_EQ(t->hours, 0);
	CHECK_EQ(t->minutes, 1);

	/* Under a minute reads as 0 h 0 min rather than hiding. */
	t = EstimateTimeLeft({8000.0l}, 30000000ULL);
	CHECK(t.has_value());
	CHECK_EQ(t->hours, 0);
	CHECK_EQ(t->minutes, 0);
}

TEST_CASE(time_left_with_nothing_free_is_zero)
{
	auto t = EstimateTimeLeft({8000.0l}, 0);
	CHECK(t.has_value());
	CHECK_EQ(t->hours, 0);
	CHECK_EQ(t->minutes, 0);
}

/* ---- Aitum Vertical ---------------------------------------------------- */

TEST_CASE(aitum_names_are_the_ones_vertical_canvas_registers)
{
	CHECK_STR(kAitumStreamPrefix, "vertical_canvas_stream");
	CHECK_STR(kAitumRecordName, "vertical_canvas_record");
	CHECK_STR(kAitumProbeProc, "aitum_vertical_get_video");
}

TEST_CASE(aitum_stream_outputs_are_matched_by_prefix)
{
	CHECK(IsAitumStreamOutput("vertical_canvas_stream"));
	CHECK(IsAitumStreamOutput("vertical_canvas_stream_Twitch"));
	CHECK(IsAitumStreamOutput("vertical_canvas_stream_"));
	CHECK(!IsAitumStreamOutput("vertical_canvas_record"));
	CHECK(!IsAitumStreamOutput("vertical_canvas"));
	CHECK(!IsAitumStreamOutput("simple_stream"));
	CHECK(!IsAitumStreamOutput(""));
	CHECK(!IsAitumStreamOutput(nullptr));
}

TEST_CASE(aitum_stream_suffix_is_the_server_name)
{
	CHECK_STR(AitumStreamSuffix("vertical_canvas_stream"), "");
	CHECK_STR(AitumStreamSuffix("vertical_canvas_stream_"), "");
	CHECK_STR(AitumStreamSuffix("vertical_canvas_stream_Twitch"), "Twitch");
	CHECK_STR(AitumStreamSuffix("vertical_canvas_stream_my_server"), "my_server");
	CHECK_STR(AitumStreamSuffix("vertical_canvas_stream__odd"), "_odd");
}

TEST_CASE(aitum_stream_title_appends_server_in_parentheses)
{
	CHECK_STR(AitumStreamTitle("Vertical Stream", "vertical_canvas_stream"), "Vertical Stream");
	CHECK_STR(AitumStreamTitle("Vertical Stream", "vertical_canvas_stream_Twitch"), "Vertical Stream (Twitch)");
	CHECK_STR(AitumStreamTitle("Vertical Stream", "vertical_canvas_stream_"), "Vertical Stream");
}

/* ---- recording path ---------------------------------------------------- */

TEST_CASE(recording_path_simple_mode_uses_simple_output_path)
{
	CHECK_STR(SelectRecordingPath("Simple", "Standard", "/ff", "/adv", "/simple"), "/simple");
	CHECK_STR(SelectRecordingPath(nullptr, "Standard", "/ff", "/adv", "/simple"), "/simple");
	CHECK_STR(SelectRecordingPath("Anything else", nullptr, nullptr, nullptr, "/simple"), "/simple");
}

TEST_CASE(recording_path_advanced_mode_picks_by_recording_type)
{
	CHECK_STR(SelectRecordingPath("Advanced", "Standard", "/ff", "/adv", "/simple"), "/adv");
	CHECK_STR(SelectRecordingPath("Advanced", "FFmpeg", "/ff", "/adv", "/simple"), "/ff");
	CHECK_STR(SelectRecordingPath("Advanced", nullptr, "/ff", "/adv", "/simple"), "/adv");
}

TEST_CASE(recording_path_is_empty_when_nothing_is_configured)
{
	CHECK_STR(SelectRecordingPath("Simple", nullptr, nullptr, nullptr, nullptr), "");
	CHECK_STR(SelectRecordingPath("Advanced", "FFmpeg", nullptr, "/adv", "/simple"), "");
	CHECK_STR(SelectRecordingPath(nullptr, nullptr, nullptr, nullptr, nullptr), "");
}

/* ---- text -------------------------------------------------------------- */

TEST_CASE(ratio_text_matches_the_built_in_panel)
{
	CHECK_STR(RatioText({3, 100, 3.0l}), "3 / 100 (3.0%)");
	CHECK_STR(RatioText({0, 0, 0.0l}), "0 / 0 (0.0%)");
	CHECK_STR(RatioText({1, 3, 33.3333l}), "1 / 3 (33.3%)");
	CHECK_STR(RatioText({4294967295u, 4294967295u, 100.0l}), "4294967295 / 4294967295 (100.0%)");
}
