/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "ui_spectrum.hpp"

#include "event_m0.hpp"

#include "spectrum_color_lut.hpp"

#include "portapack.hpp"
#include "portapack_shared_memory.hpp"
using namespace portapack;

#include "string_format.hpp"

#include <cmath>
#include <array>

namespace ui {
namespace spectrum {

/* FrequencyScale ********************************************************/

void FrequencyScale::on_show() {
	clear();
}

void FrequencyScale::set_spectrum_sampling_rate(const uint32_t new_sampling_rate) {
	if( (spectrum_sampling_rate != new_sampling_rate) ) {
		spectrum_sampling_rate = new_sampling_rate;
		set_dirty();
	}
}

void FrequencyScale::set_channel_filter(
	const uint32_t pass_frequency,
	const uint32_t stop_frequency
) {
	if( (channel_filter_pass_frequency != pass_frequency) ||
		(channel_filter_stop_frequency != stop_frequency) ) {
		channel_filter_pass_frequency = pass_frequency;
		channel_filter_stop_frequency = stop_frequency;
		set_dirty();
	}
}

void FrequencyScale::paint(Painter& painter) {
	const auto r = screen_rect();

	clear_background(painter, r);

	if( !spectrum_sampling_rate ) {
		// Can't draw without non-zero scale.
		return;
	}

	draw_filter_ranges(painter, r);
	draw_frequency_ticks(painter, r);
}

void FrequencyScale::clear() {
	spectrum_sampling_rate = 0;
	set_dirty();
}

void FrequencyScale::clear_background(Painter& painter, const Rect r) {
	painter.fill_rectangle(r, Color::black());
}

void FrequencyScale::draw_frequency_ticks(Painter& painter, const Rect r) {
	const auto x_center = r.width() / 2;

	const Rect tick { r.left() + x_center, r.top(), 1, r.height() };
	painter.fill_rectangle(tick, Color::white());

	constexpr uint32_t tick_count_max = 4;
	float rough_tick_interval = float(spectrum_sampling_rate) / tick_count_max;
	size_t magnitude = 1;
	size_t magnitude_n = 0;
	while(rough_tick_interval >= 10.0f) {
		rough_tick_interval /= 10;
		magnitude *= 10;
		magnitude_n += 1;
	}
	const size_t tick_interval = std::ceil(rough_tick_interval);

	size_t tick_offset = tick_interval;
	while((tick_offset * magnitude) < spectrum_sampling_rate / 2) {
		const Dim pixel_offset = tick_offset * magnitude * spectrum_bins / spectrum_sampling_rate;

		const std::string zero_pad =
			((magnitude_n % 3) == 0) ? "" :
			((magnitude_n % 3) == 1) ? "0" : "00";
		const std::string unit =
			(magnitude_n >= 6) ? "M" :
			(magnitude_n >= 3) ? "k" : "";
		const std::string label = to_string_dec_uint(tick_offset) + zero_pad + unit;
		
		const Coord offset_low = r.left() + x_center - pixel_offset;
		const Rect tick_low { offset_low, r.top(), 1, r.height() };
		painter.fill_rectangle(tick_low, Color::white());
		painter.draw_string({ offset_low + 2, r.top() }, style(), label );

		const Coord offset_high = r.left() + x_center + pixel_offset;
		const Rect tick_high { offset_high, r.top(), 1, r.height() };
		painter.fill_rectangle(tick_high, Color::white());
		painter.draw_string({ offset_high + 2, r.top() }, style(), label );

		tick_offset += tick_interval;
	}
}

void FrequencyScale::draw_filter_ranges(Painter& painter, const Rect r) {
	if( channel_filter_pass_frequency ) {
		const auto x_center = r.width() / 2;

		const auto pass_offset = channel_filter_pass_frequency * spectrum_bins / spectrum_sampling_rate;
		const auto stop_offset = channel_filter_stop_frequency * spectrum_bins / spectrum_sampling_rate;

		const auto pass_x_lo = x_center - pass_offset;
		const auto pass_x_hi = x_center + pass_offset;

		if( channel_filter_stop_frequency ) {
			const auto stop_x_lo = x_center - stop_offset;
			const auto stop_x_hi = x_center + stop_offset;

			const Rect r_stop_lo {
				r.left() + stop_x_lo, r.bottom() - filter_band_height,
				pass_x_lo - stop_x_lo, filter_band_height
			};
			painter.fill_rectangle(
				r_stop_lo,
				Color::yellow()
			);

			const Rect r_stop_hi {
				r.left() + pass_x_hi, r.bottom() - filter_band_height,
				stop_x_hi - pass_x_hi, filter_band_height
			};
			painter.fill_rectangle(
				r_stop_hi,
				Color::yellow()
			);
		}

		const Rect r_pass {
			r.left() + pass_x_lo, r.bottom() - filter_band_height,
			pass_x_hi - pass_x_lo, filter_band_height
		};
		painter.fill_rectangle(
			r_pass,
			Color::green()
		);
	}
}

/* WaterfallView *********************************************************/

void WaterfallView::on_show() {
	clear();

	const auto screen_r = screen_rect();
	display.scroll_set_area(screen_r.top(), screen_r.bottom());
}

void WaterfallView::on_hide() {
	/* TODO: Clear region to eliminate brief flash of content at un-shifted
	 * position?
	 */
	display.scroll_disable();
}

void WaterfallView::paint(Painter& painter) {
	// Do nothing.
	(void)painter;
}

void WaterfallView::on_channel_spectrum(
	const ChannelSpectrum& spectrum
) {
	/* TODO: static_assert that message.spectrum.db.size() >= pixel_row.size() */

	std::array<Color, 240> pixel_row;
	for(size_t i=0; i<120; i++) {
		const auto pixel_color = spectrum_rgb3_lut[spectrum.db[256 - 120 + i]];
		pixel_row[i] = pixel_color;
	}

	for(size_t i=120; i<240; i++) {
		const auto pixel_color = spectrum_rgb3_lut[spectrum.db[i - 120]];
		pixel_row[i] = pixel_color;
	}

	const auto draw_y = display.scroll(1);

	display.draw_pixels(
		{ { 0, draw_y }, { pixel_row.size(), 1 } },
		pixel_row
	);
}

void WaterfallView::clear() {
	display.fill_rectangle(
		screen_rect(),
		Color::black()
	);
}

/* WaterfallWidget *******************************************************/

WaterfallWidget::WaterfallWidget() {
	add_children({
		&waterfall_view,
		&frequency_scale,
	});
}

void WaterfallWidget::on_show() {
	EventDispatcher::message_map().register_handler(Message::ID::ChannelSpectrumConfig,
		[this](const Message* const p) {
			const auto message = *reinterpret_cast<const ChannelSpectrumConfigMessage*>(p);
			this->fifo = message.fifo;
		}
	);
	EventDispatcher::message_map().register_handler(Message::ID::DisplayFrameSync,
		[this](const Message* const) {
			if( this->fifo ) {
				ChannelSpectrum channel_spectrum;
				while( fifo->out(channel_spectrum) ) {
					this->on_channel_spectrum(channel_spectrum);
				}
			}
		}
	);

	shared_memory.baseband_queue.push_and_wait(
		SpectrumStreamingConfigMessage {
			SpectrumStreamingConfigMessage::Mode::Running
		}
	);
}

void WaterfallWidget::on_hide() {
	shared_memory.baseband_queue.push_and_wait(
		SpectrumStreamingConfigMessage {
			SpectrumStreamingConfigMessage::Mode::Stopped
		}
	);

	EventDispatcher::message_map().unregister_handler(Message::ID::DisplayFrameSync);
	EventDispatcher::message_map().unregister_handler(Message::ID::ChannelSpectrumConfig);
}

void WaterfallWidget::set_parent_rect(const Rect new_parent_rect) {
	constexpr Dim scale_height = 20;

	View::set_parent_rect(new_parent_rect);
	frequency_scale.set_parent_rect({ 0, 0, new_parent_rect.width(), scale_height });
	waterfall_view.set_parent_rect({
		0, scale_height,
		new_parent_rect.width(),
		new_parent_rect.height() - scale_height
	});
}

void WaterfallWidget::paint(Painter& painter) {
	// TODO:
	(void)painter;
}

void WaterfallWidget::on_channel_spectrum(const ChannelSpectrum& spectrum) {
	waterfall_view.on_channel_spectrum(spectrum);
	frequency_scale.set_spectrum_sampling_rate(spectrum.sampling_rate);
	frequency_scale.set_channel_filter(
		spectrum.channel_filter_pass_frequency,
		spectrum.channel_filter_stop_frequency
	);
}

} /* namespace spectrum */
} /* namespace ui */
