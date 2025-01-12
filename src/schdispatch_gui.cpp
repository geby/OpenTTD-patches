/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file schdispatch_gui.cpp GUI code for Scheduled Dispatch */

#include "stdafx.h"
#include "command_func.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "strings_func.h"
#include "vehicle_base.h"
#include "string_func.h"
#include "spritecache.h"
#include "gfx_func.h"
#include "company_func.h"
#include "date_func.h"
#include "date_gui.h"
#include "vehicle_gui.h"
#include "settings_type.h"
#include "viewport_func.h"
#include "zoom_func.h"
#include "core/geometry_func.hpp"

#include <vector>
#include <algorithm>

#include "table/strings.h"
#include "table/string_colours.h"
#include "table/sprites.h"

#include "safeguards.h"

enum SchdispatchWidgets {
	WID_SCHDISPATCH_CAPTION,         ///< Caption of window.
	WID_SCHDISPATCH_MATRIX,          ///< Matrix of vehicles.
	WID_SCHDISPATCH_V_SCROLL,        ///< Vertical scrollbar.
    WID_SCHDISPATCH_SUMMARY_PANEL,   ///< Summary panel

	WID_SCHDISPATCH_ENABLED,         ///< Enable button.
	WID_SCHDISPATCH_ADD,             ///< Add Departure Time button
	WID_SCHDISPATCH_SET_DURATION,    ///< Duration button
	WID_SCHDISPATCH_SET_START_DATE,  ///< Start Date button
	WID_SCHDISPATCH_SET_DELAY,       ///< Delat button
	WID_SCHDISPATCH_RESET_DISPATCH,  ///< Reset dispatch button
};

/**
 * Callback for when a time has been chosen to start the schedule
 * @param windex The windows index
 * @param date the actually chosen date
 */
static void SetScheduleStartDateIntl(uint32 windex, DateTicksScaled date)
{
	Date start_date;
	uint16 start_full_date_fract;
	SchdispatchConvertToFullDateFract(date, &start_date, &start_full_date_fract);

	uint32 p1 = 0, p2 = 0;
	SB(p1, 0, 20, windex);
	SB(p1, 20, 12, GB(start_full_date_fract, 2, 12));
	SB(p2, 0, 30, start_date);
	SB(p2, 30, 2, GB(start_full_date_fract, 0, 2));

	DoCommandP(0, p1, p2, CMD_SCHEDULED_DISPATCH_SET_START_DATE | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
}

/**
 * Callback for when a time has been chosen to start the schedule
 * @param window the window related to the setting of the date
 * @param date the actually chosen date
 */
static void SetScheduleStartDateCallback(const Window *w, DateTicksScaled date)
{
	SetScheduleStartDateIntl(w->window_number, date);
}

/**
 * Callback for when a time has been chosen to add to the schedule
 * @param p1 The p1 parameter to send to CmdScheduledDispatchAdd
 * @param date the actually chosen date
 */
static void ScheduleAddIntl(uint32 p1, DateTicksScaled date, uint extra_slots, uint offset)
{
	VehicleID veh = GB(p1, 0, 20);
	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle()) return;

	/* Make sure the time is the closest future to the timetable start */
	DateTicksScaled start_tick = v->orders.list->GetScheduledDispatchStartTick();
	uint32 duration = v->orders.list->GetScheduledDispatchDuration();
	while (date > start_tick) date -= duration;
	while (date < start_tick) date += duration;

	if (extra_slots > 0 && offset > 0) {
		DateTicksScaled end_tick = start_tick + duration;
		DateTicksScaled max_extra_slots = (end_tick - 1 - date) / offset;
		if (max_extra_slots < extra_slots) extra_slots = static_cast<uint>(std::max<DateTicksScaled>(0, max_extra_slots));
		extra_slots = std::min<uint>(extra_slots, UINT16_MAX);
	}

	DoCommandPEx(0, v->index, (uint32)(date - start_tick), (((uint64)extra_slots) << 32) | offset, CMD_SCHEDULED_DISPATCH_ADD | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE), nullptr, nullptr, 0);
}

/**
 * Callback for when a time has been chosen to add to the schedule
 * @param window the window related to the setting of the date
 * @param date the actually chosen date
 */
static void ScheduleAddCallback(const Window *w, DateTicksScaled date)
{
	ScheduleAddIntl(w->window_number, date, 0, 0);
}

/**
 * Calculate the maximum number of vehicle required to run this timetable according to the dispatch schedule
 * @param timetable_duration  timetable duration in scaled tick
 * @param schedule_duration  scheduled dispatch duration in scaled tick
 * @param offsets list of all dispatch offsets in the schedule
 * @return maxinum number of vehicle required
 */
static int CalculateMaxRequiredVehicle(Ticks timetable_duration, uint32 schedule_duration, std::vector<uint32> offsets)
{
	if (timetable_duration == INVALID_TICKS) return -1;
	if (offsets.size() == 0) return -1;

	/* Number of time required to ensure all vehicle are counted */
	int required_loop = CeilDiv(timetable_duration, schedule_duration) + 1;

	/* Create indice array to count maximum overlapping range */
	std::vector<std::pair<uint32, int>> indices;
	for (int i = 0; i < required_loop; i++) {
		for (uint32 offset : offsets) {
			if (offset >= schedule_duration) continue;
			indices.push_back(std::make_pair(i * schedule_duration + offset, 1));
			indices.push_back(std::make_pair(i * schedule_duration + offset + timetable_duration, -1));
		}
	}
	if (indices.empty()) return -1;
	std::sort(indices.begin(), indices.end());
	int current_count = 0;
	int vehicle_count = 0;
	for (const auto& inc : indices) {
		current_count += inc.second;
		if (current_count > vehicle_count) vehicle_count = current_count;
	}
	return vehicle_count;
}

struct SchdispatchWindow : Window {
	const Vehicle *vehicle; ///< Vehicle monitored by the window.
	int clicked_widget;     ///< The widget that was clicked (used to determine what to do in OnQueryTextFinished)
	Scrollbar *vscroll;     ///< Verticle scrollbar
	uint num_columns;       ///< Number of columns.

	uint item_count = 0;     ///< Number of scheduled item
	bool last_departure_future; ///< True if last departure is currently displayed in the future
	uint warning_count = 0;

	SchdispatchWindow(WindowDesc *desc, WindowNumber window_number) :
			Window(desc),
			vehicle(Vehicle::Get(window_number))
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SCHDISPATCH_V_SCROLL);
		this->FinishInitNested(window_number);

		this->owner = this->vehicle->owner;
	}

	~SchdispatchWindow()
	{
		if (!FocusWindowById(WC_VEHICLE_VIEW, this->window_number)) {
			MarkAllRouteStepsDirty(this->vehicle);
		}
	}

	uint base_width;
	uint header_width;
	uint flag_width;
	uint flag_height;

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_SCHDISPATCH_MATRIX: {
				uint min_height = 0;

				SetDParamMaxValue(0, _settings_time.time_in_minutes ? 0 : MAX_YEAR * DAYS_IN_YEAR);
				Dimension unumber = GetStringBoundingBox(STR_JUST_DATE_WALLCLOCK_TINY);
				const Sprite *spr = GetSprite(SPR_FLAG_VEH_STOPPED, ST_NORMAL);
				this->flag_width  = UnScaleGUI(spr->width) + WD_FRAMERECT_RIGHT;
				this->flag_height = UnScaleGUI(spr->height);

				min_height = std::max<uint>(unumber.height + WD_MATRIX_TOP, UnScaleGUI(spr->height));
				this->header_width = this->flag_width + WD_FRAMERECT_LEFT;
				this->base_width = unumber.width + this->header_width + 4;

				resize->height = min_height;
				resize->width = base_width;
				size->width = resize->width * 3;
				size->height = resize->height * 3;

				fill->width = resize->width;
				fill->height = resize->height;
				break;
			}

			case WID_SCHDISPATCH_SUMMARY_PANEL:
				size->height = WD_FRAMERECT_TOP + 5 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				if (warning_count > 0) {
					const Dimension warning_dimensions = GetSpriteSize(SPR_WARNING_SIGN);
					size->height += warning_count * std::max<int>(warning_dimensions.height, FONT_HEIGHT_NORMAL);
				}
				break;
		}
	}

	/**
	 * Set proper item_count to number of offsets in the schedule.
	 */
	void CountItem()
	{
		this->item_count = 0;
		if (this->vehicle->orders.list != nullptr) {
			this->item_count = (uint)this->vehicle->orders.list->GetScheduledDispatch().size();
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		switch (data) {
			case VIWD_MODIFY_ORDERS:
				if (!gui_scope) break;
				this->ReInit();
				break;
		}
	}

	virtual void OnPaint() override
	{
		const Vehicle *v = this->vehicle;
		CountItem();

		this->SetWidgetDisabledState(WID_SCHDISPATCH_ENABLED, (v->owner != _local_company) || HasBit(v->vehicle_flags, VF_TIMETABLE_SEPARATION));

		bool disabled = (v->owner != _local_company) || !HasBit(v->vehicle_flags, VF_SCHEDULED_DISPATCH) || (v->orders.list == nullptr);
		this->SetWidgetDisabledState(WID_SCHDISPATCH_ADD, disabled);
		this->SetWidgetDisabledState(WID_SCHDISPATCH_SET_DURATION, disabled);
		this->SetWidgetDisabledState(WID_SCHDISPATCH_SET_START_DATE, disabled);
		this->SetWidgetDisabledState(WID_SCHDISPATCH_SET_DELAY, disabled);
		this->SetWidgetDisabledState(WID_SCHDISPATCH_RESET_DISPATCH, disabled);

		this->vscroll->SetCount(CeilDiv(this->item_count, this->num_columns));

		this->SetWidgetLoweredState(WID_SCHDISPATCH_ENABLED, HasBit(v->vehicle_flags, VF_SCHEDULED_DISPATCH));
		this->DrawWidgets();
	}

	virtual void SetStringParameters(int widget) const override
	{
		switch (widget) {
			case WID_SCHDISPATCH_CAPTION: SetDParam(0, this->vehicle->index); break;
		}
	}

	virtual bool OnTooltip(Point pt, int widget, TooltipCloseCondition close_cond) override
	{
		switch (widget) {
			case WID_SCHDISPATCH_ADD: {
				if (_settings_time.time_in_minutes) {
					uint64 params[1];
					params[0] = STR_SCHDISPATCH_ADD_TOOLTIP;
					GuiShowTooltips(this, STR_SCHDISPATCH_ADD_TOOLTIP_EXTRA, 1, params, close_cond);
					return true;
				}
				break;
			}

			default:
				break;
		}

		return false;
	}

	/**
	 * Draw a time in the box with the top left corner at x,y.
	 * @param time  Time to draw.
	 * @param left  Left side of the box to draw in.
	 * @param right Right side of the box to draw in.
	 * @param y     Top of the box to draw in.
	 */
	void DrawScheduledTime(const DateTicksScaled time, int left, int right, int y, TextColour colour) const
	{
		bool rtl = _current_text_dir == TD_RTL;
		uint diff_x, diff_y;
		diff_x = this->flag_width + WD_FRAMERECT_LEFT;
		diff_y = (this->resize.step_height - this->flag_height) / 2 - 2;

		int text_left  = rtl ? right - this->base_width - 1 : left + diff_x;
		int text_right = rtl ? right - diff_x : left + this->base_width - 1;

		DrawSprite(SPR_FLAG_VEH_STOPPED, PAL_NONE, rtl ? right - this->flag_width : left + WD_FRAMERECT_LEFT, y + diff_y);

		SetDParam(0, time);
		DrawString(text_left, text_right, y + 2, STR_JUST_DATE_WALLCLOCK_TINY, colour);
	}

	virtual void OnGameTick() override
	{
		const Vehicle *v = this->vehicle;
		if (HasBit(v->vehicle_flags, VF_SCHEDULED_DISPATCH) && v->orders.list != nullptr) {
			if (((v->orders.list->GetScheduledDispatchStartTick() + v->orders.list->GetScheduledDispatchLastDispatch()) > _scaled_date_ticks) != this->last_departure_future) {
				SetWidgetDirty(WID_SCHDISPATCH_SUMMARY_PANEL);
			}
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const override
	{
		const Vehicle *v = this->vehicle;

		switch (widget) {
			case WID_SCHDISPATCH_MATRIX: {
				/* If order is not initialized, don't draw */
				if (v->orders.list == nullptr) break;

				bool rtl = _current_text_dir == TD_RTL;

				/* Set the row and number of boxes in each row based on the number of boxes drawn in the matrix */
				const NWidgetCore *wid = this->GetWidget<NWidgetCore>(WID_SCHDISPATCH_MATRIX);
				const uint16 rows_in_display = wid->current_y / wid->resize_y;

				uint num = this->vscroll->GetPosition() * this->num_columns;
				if (num >= v->orders.list->GetScheduledDispatch().size()) break;

				const uint maxval = std::min<uint>(this->item_count, num + (rows_in_display * this->num_columns));

				auto current_schedule = v->orders.list->GetScheduledDispatch().begin() + num;
				const DateTicksScaled start_tick = v->orders.list->GetScheduledDispatchStartTick();
				const DateTicksScaled end_tick = v->orders.list->GetScheduledDispatchStartTick() + v->orders.list->GetScheduledDispatchDuration();

				for (int y = r.top + 1; num < maxval; y += this->resize.step_height) { /* Draw the rows */
					for (byte i = 0; i < this->num_columns && num < maxval; i++, num++) {
						/* Draw all departure time in the current row */
						if (current_schedule != v->orders.list->GetScheduledDispatch().end()) {
							int x = r.left + (rtl ? (this->num_columns - i - 1) : i) * this->resize.step_width;
							DateTicksScaled draw_time = start_tick + *current_schedule;
							this->DrawScheduledTime(draw_time, x, x + this->resize.step_width - 1, y, draw_time >= end_tick ? TC_RED : TC_BLACK);
							current_schedule++;
						} else {
							break;
						}
					}
				}
				break;
			}

			case WID_SCHDISPATCH_SUMMARY_PANEL: {

				int y = r.top + WD_FRAMERECT_TOP;

				if (!HasBit(v->vehicle_flags, VF_SCHEDULED_DISPATCH) || v->orders.list == nullptr) {
					y += FONT_HEIGHT_NORMAL;
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_SCHDISPATCH_SUMMARY_NOT_ENABLED);
				} else {

					const DateTicksScaled last_departure = v->orders.list->GetScheduledDispatchStartTick() + v->orders.list->GetScheduledDispatchLastDispatch();
					SetDParam(0, last_departure);
					const_cast<SchdispatchWindow*>(this)->last_departure_future = (last_departure > _scaled_date_ticks);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y,
							this->last_departure_future ? STR_SCHDISPATCH_SUMMARY_LAST_DEPARTURE_FUTURE : STR_SCHDISPATCH_SUMMARY_LAST_DEPARTURE_PAST);
					y += FONT_HEIGHT_NORMAL;

					bool have_conditional = false;
					for (int n = 0; n < v->GetNumOrders(); n++) {
						const Order *order = v->GetOrder(n);
						if (order->IsType(OT_CONDITIONAL)) {
							have_conditional = true;
						}
					}
					if (!have_conditional) {
						const int required_vehicle = CalculateMaxRequiredVehicle(v->orders.list->GetTimetableTotalDuration(), v->orders.list->GetScheduledDispatchDuration(), v->orders.list->GetScheduledDispatch());
						if (required_vehicle > 0) {
							SetDParam(0, required_vehicle);
							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_SCHDISPATCH_SUMMARY_L1);
						}
					}
					y += FONT_HEIGHT_NORMAL;

					SetTimetableParams(0, v->orders.list->GetScheduledDispatchDuration(), true);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_SCHDISPATCH_SUMMARY_L2);
					y += FONT_HEIGHT_NORMAL;

					SetDParam(0, v->orders.list->GetScheduledDispatchStartTick());
					SetDParam(1, v->orders.list->GetScheduledDispatchStartTick() + v->orders.list->GetScheduledDispatchDuration());
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_SCHDISPATCH_SUMMARY_L3);
					y += FONT_HEIGHT_NORMAL;

					SetTimetableParams(0, v->orders.list->GetScheduledDispatchDelay());
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_SCHDISPATCH_SUMMARY_L4);
					y += FONT_HEIGHT_NORMAL;

					uint warnings = 0;
					auto draw_warning = [&](StringID text) {
						const Dimension warning_dimensions = GetSpriteSize(SPR_WARNING_SIGN);
						int step_height = std::max<int>(warning_dimensions.height, FONT_HEIGHT_NORMAL);
						int left = r.left + WD_FRAMERECT_LEFT;
						int right = r.right - WD_FRAMERECT_RIGHT;
						const bool rtl = (_current_text_dir == TD_RTL);
						DrawSprite(SPR_WARNING_SIGN, 0, rtl ? right - warning_dimensions.width - 5 : left + 5, y + (step_height - warning_dimensions.height) / 2);
						if (rtl) {
							right -= (warning_dimensions.width + 10);
						} else {
							left += (warning_dimensions.width + 10);
						}
						DrawString(left, right, y + (step_height - FONT_HEIGHT_NORMAL) / 2, text);
						y += step_height;
						warnings++;
					};

					uint32 duration = v->orders.list->GetScheduledDispatchDuration();
					for (uint32 slot : v->orders.list->GetScheduledDispatch()) {
						if (slot >= duration) {
							draw_warning(STR_SCHDISPATCH_SLOT_OUTSIDE_SCHEDULE);
							break;
						}
					}

					if (warnings != this->warning_count) {
						SchdispatchWindow *mutable_this = const_cast<SchdispatchWindow *>(this);
						mutable_this->warning_count = warnings;
						mutable_this->ReInit();
					}
				}

				break;
			}
		}
	}

	/**
	 * Handle click in the departure time matrix.
	 * @param x Horizontal position in the matrix widget in pixels.
	 * @param y Vertical position in the matrix widget in pixels.
	 */
	void TimeClick(int x, int y)
	{
		const NWidgetCore *matrix_widget = this->GetWidget<NWidgetCore>(WID_SCHDISPATCH_MATRIX);
		/* In case of RTL the widgets are swapped as a whole */
		if (_current_text_dir == TD_RTL) x = matrix_widget->current_x - x;

		uint xt = 0, xm = 0;
		xt = x / this->resize.step_width;
		xm = x % this->resize.step_width;
		if (xt >= this->num_columns) return;

		uint row = y / this->resize.step_height;
		if (row >= this->vscroll->GetCapacity()) return;

		uint pos = ((row + this->vscroll->GetPosition()) * this->num_columns) + xt;

		if (pos >= this->item_count) return;

		if (xm <= this->header_width) {
			DoCommandP(0, this->vehicle->index, this->vehicle->orders.list->GetScheduledDispatch()[pos], CMD_SCHEDULED_DISPATCH_REMOVE | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
		}
	}

	int32 ProcessDurationForQueryString(int32 duration) const
	{
		if (!_settings_client.gui.timetable_in_ticks) duration = RoundDivSU(duration, DATE_UNIT_SIZE);
		return duration;
	}

	int GetQueryStringCaptionOffset() const
	{
		if (_settings_client.gui.timetable_in_ticks) return 2;
		if (_settings_time.time_in_minutes) return 0;
		return 1;
	}

	virtual void OnClick(Point pt, int widget, int click_count) override
	{
		const Vehicle *v = this->vehicle;

		this->clicked_widget = widget;
		this->DeleteChildWindows(WC_QUERY_STRING);

		switch (widget) {
			case WID_SCHDISPATCH_MATRIX: { /* List */
				NWidgetBase *nwi = this->GetWidget<NWidgetBase>(WID_SCHDISPATCH_MATRIX);
				this->TimeClick(pt.x - nwi->pos_x, pt.y - nwi->pos_y);
				break;
			}

			case WID_SCHDISPATCH_ENABLED: {
				uint32 p2 = 0;
				if (!HasBit(v->vehicle_flags, VF_SCHEDULED_DISPATCH)) SetBit(p2, 0);

				if (!v->orders.list->IsScheduledDispatchValid()) v->orders.list->ResetScheduledDispatch();
				DoCommandP(0, v->index, p2, CMD_SCHEDULED_DISPATCH | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
				break;
			}

			case WID_SCHDISPATCH_ADD: {
				if (_settings_time.time_in_minutes && _ctrl_pressed) {
					void ShowScheduledDispatchAddSlotsWindow(SchdispatchWindow *parent, int window_number);
					ShowScheduledDispatchAddSlotsWindow(this, v->index);
				} else if (_settings_time.time_in_minutes && _settings_client.gui.timetable_start_text_entry) {
					ShowQueryString(STR_EMPTY, STR_SCHDISPATCH_ADD_CAPTION, 31, this, CS_NUMERAL, QSF_NONE);
				} else {
					ShowSetDateWindow(this, v->index, _scaled_date_ticks, _cur_year, _cur_year + 15, ScheduleAddCallback, STR_SCHDISPATCH_ADD, STR_SCHDISPATCH_ADD_TOOLTIP);
				}
				break;
			}

			case WID_SCHDISPATCH_SET_DURATION: {
				SetDParam(0, ProcessDurationForQueryString(v->orders.list->GetScheduledDispatchDuration()));
				ShowQueryString(STR_JUST_INT, STR_SCHDISPATCH_DURATION_CAPTION_MINUTE + this->GetQueryStringCaptionOffset(), 31, this, CS_NUMERAL, QSF_NONE);
				break;
			}

			case WID_SCHDISPATCH_SET_START_DATE: {
				if (_settings_time.time_in_minutes && _settings_client.gui.timetable_start_text_entry) {
					uint64 time = _scaled_date_ticks;
					time /= _settings_time.ticks_per_minute;
					time += _settings_time.clock_offset;
					time %= (24 * 60);
					time = (time % 60) + (((time / 60) % 24) * 100);
					SetDParam(0, time);
					ShowQueryString(STR_JUST_INT, STR_SCHDISPATCH_START_CAPTION_MINUTE, 31, this, CS_NUMERAL, QSF_ACCEPT_UNCHANGED);
				} else {
					ShowSetDateWindow(this, v->index, _scaled_date_ticks, _cur_year, _cur_year + 15, SetScheduleStartDateCallback, STR_SCHDISPATCH_SET_START, STR_SCHDISPATCH_START_TOOLTIP);
				}
				break;
			}

			case WID_SCHDISPATCH_SET_DELAY: {
				SetDParam(0, ProcessDurationForQueryString(v->orders.list->GetScheduledDispatchDelay()));
				ShowQueryString(STR_JUST_INT, STR_SCHDISPATCH_DELAY_CAPTION_MINUTE + this->GetQueryStringCaptionOffset(), 31, this, CS_NUMERAL, QSF_NONE);
				break;
			}

			case WID_SCHDISPATCH_RESET_DISPATCH: {
				DoCommandP(0, v->index, 0, CMD_SCHEDULED_DISPATCH_RESET_LAST_DISPATCH | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
				break;
			}
		}

		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;
		const Vehicle *v = this->vehicle;

		switch (this->clicked_widget) {
			default: NOT_REACHED();

			case WID_SCHDISPATCH_ADD: {
				char *end;
				int32 val = StrEmpty(str) ? -1 : strtoul(str, &end, 10);

				if (val >= 0 && end && *end == 0) {
					uint minutes = (val % 100) % 60;
					uint hours = (val / 100) % 24;
					DateTicksScaled slot = MINUTES_DATE(MINUTES_DAY(CURRENT_MINUTE), hours, minutes);
					slot -= _settings_time.clock_offset;
					slot *= _settings_time.ticks_per_minute;
					ScheduleAddIntl(v->index, slot, 0, 0);
				}
				break;
			}

			case WID_SCHDISPATCH_SET_START_DATE: {
				char *end;
				int32 val = StrEmpty(str) ? -1 : strtoul(str, &end, 10);

				if (val >= 0 && end && *end == 0) {
					uint minutes = (val % 100) % 60;
					uint hours = (val / 100) % 24;
					DateTicksScaled start = MINUTES_DATE(MINUTES_DAY(CURRENT_MINUTE), hours, minutes);
					start -= _settings_time.clock_offset;
					start *= _settings_time.ticks_per_minute;
					SetScheduleStartDateIntl(v->index, start);
				}
				break;
			}

			case WID_SCHDISPATCH_SET_DURATION: {
				int32 val = StrEmpty(str) ? 0 : strtoul(str, nullptr, 10);

				if (val > 0) {
					if (!_settings_client.gui.timetable_in_ticks) val *= DATE_UNIT_SIZE;

					DoCommandP(0, v->index, val, CMD_SCHEDULED_DISPATCH_SET_DURATION | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
				}
				break;
			}

			case WID_SCHDISPATCH_SET_DELAY: {
				char *end;
				int32 val = StrEmpty(str) ? -1 : strtoul(str, &end, 10);

				if (val >= 0 && end && *end == 0) {
					if (!_settings_client.gui.timetable_in_ticks) val *= DATE_UNIT_SIZE;

					DoCommandP(0, v->index, val, CMD_SCHEDULED_DISPATCH_SET_DELAY | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
				}
				break;
			}
		}

		this->SetDirty();
	}

	virtual void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SCHDISPATCH_MATRIX);
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(WID_SCHDISPATCH_MATRIX);
		this->num_columns = nwi->current_x / nwi->resize_x;
	}

	virtual void OnFocus(Window *previously_focused_window) override
	{
		if (HasFocusedVehicleChanged(this->window_number, previously_focused_window)) {
			MarkAllRoutePathsDirty(this->vehicle);
			MarkAllRouteStepsDirty(this->vehicle);
		}
	}

	const Vehicle *GetVehicle()
	{
		return this->vehicle;
	}

	void AddMultipleDepartureSlots(uint start, uint step, uint end)
	{
		if (end < start || step == 0) return;

		DateTicksScaled slot = MINUTES_DATE(MINUTES_DAY(CURRENT_MINUTE), 0, start);
		slot -= _settings_time.clock_offset;
		slot *= _settings_time.ticks_per_minute;
		ScheduleAddIntl(this->vehicle->index, slot, (end - start) / step, step * _settings_time.ticks_per_minute);
	}
};

static const NWidgetPart _nested_schdispatch_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SCHDISPATCH_CAPTION), SetDataTip(STR_SCHDISPATCH_CAPTION, STR_NULL),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_SCHDISPATCH_MATRIX), SetResize(1, 1), SetScrollbar(WID_SCHDISPATCH_V_SCROLL),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SCHDISPATCH_V_SCROLL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_SCHDISPATCH_SUMMARY_PANEL), SetMinimalSize(400, 22), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(NWID_VERTICAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCHDISPATCH_ENABLED), SetDataTip(STR_SCHDISPATCH_ENABLED, STR_SCHDISPATCH_ENABLED_TOOLTIP), SetFill(1, 1), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCHDISPATCH_ADD), SetDataTip(STR_SCHDISPATCH_ADD, STR_SCHDISPATCH_ADD_TOOLTIP), SetFill(1, 1), SetResize(1, 0),
		EndContainer(),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCHDISPATCH_SET_DURATION), SetDataTip(STR_SCHDISPATCH_DURATION, STR_SCHDISPATCH_DURATION_TOOLTIP), SetFill(1, 1), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCHDISPATCH_SET_START_DATE), SetDataTip(STR_SCHDISPATCH_START, STR_SCHDISPATCH_START_TOOLTIP), SetFill(1, 1), SetResize(1, 0),
		EndContainer(),
		NWidget(NWID_VERTICAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCHDISPATCH_SET_DELAY), SetDataTip(STR_SCHDISPATCH_DELAY, STR_SCHDISPATCH_DELAY_TOOLTIP), SetFill(1, 1), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCHDISPATCH_RESET_DISPATCH), SetDataTip(STR_SCHDISPATCH_RESET_LAST_DISPATCH, STR_SCHDISPATCH_RESET_LAST_DISPATCH_TOOLTIP), SetFill(1, 1), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _schdispatch_desc(
	WDP_AUTO, "scheduled_dispatch_slots", 400, 130,
	WC_SCHDISPATCH_SLOTS, WC_VEHICLE_TIMETABLE,
	WDF_CONSTRUCTION,
	_nested_schdispatch_widgets, lengthof(_nested_schdispatch_widgets)
);

/**
 * Show the slot dispatching slots
 * @param v The vehicle to show the slot dispatching slots for
 */
void ShowSchdispatchWindow(const Vehicle *v)
{
	AllocateWindowDescFront<SchdispatchWindow>(&_schdispatch_desc, v->index);
}

enum ScheduledDispatchAddSlotsWindowWidgets {
	WID_SCHDISPATCH_ADD_SLOT_START_HOUR,
	WID_SCHDISPATCH_ADD_SLOT_START_MINUTE,
	WID_SCHDISPATCH_ADD_SLOT_STEP_HOUR,
	WID_SCHDISPATCH_ADD_SLOT_STEP_MINUTE,
	WID_SCHDISPATCH_ADD_SLOT_END_HOUR,
	WID_SCHDISPATCH_ADD_SLOT_END_MINUTE,
	WID_SCHDISPATCH_ADD_SLOT_ADD_BUTTON,
	WID_SCHDISPATCH_ADD_SLOT_START_TEXT,
	WID_SCHDISPATCH_ADD_SLOT_STEP_TEXT,
	WID_SCHDISPATCH_ADD_SLOT_END_TEXT,
};

struct ScheduledDispatchAddSlotsWindow : Window {
	uint start;
	uint step;
	uint end;

	ScheduledDispatchAddSlotsWindow(WindowDesc *desc, WindowNumber window_number, SchdispatchWindow *parent) :
			Window(desc)
	{
		this->start = (_scaled_date_ticks / _settings_time.ticks_per_minute) % (60 * 24);
		this->step = 30;
		this->end = this->start + 60;
		this->parent = parent;
		this->CreateNestedTree();
		this->FinishInitNested(window_number);
	}

	Point OnInitialPosition(int16 sm_width, int16 sm_height, int window_number) override
	{
		Point pt = { this->parent->left + this->parent->width / 2 - sm_width / 2, this->parent->top + this->parent->height / 2 - sm_height / 2 };
		return pt;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		Dimension d = {0, 0};
		switch (widget) {
			default: return;

			case WID_SCHDISPATCH_ADD_SLOT_START_TEXT:
			case WID_SCHDISPATCH_ADD_SLOT_STEP_TEXT:
			case WID_SCHDISPATCH_ADD_SLOT_END_TEXT:
				d = maxdim(d, GetStringBoundingBox(STR_SCHDISPATCH_ADD_DEPARTURE_SLOTS_START));
				d = maxdim(d, GetStringBoundingBox(STR_SCHDISPATCH_ADD_DEPARTURE_SLOTS_STEP));
				d = maxdim(d, GetStringBoundingBox(STR_SCHDISPATCH_ADD_DEPARTURE_SLOTS_END));
				break;

			case WID_SCHDISPATCH_ADD_SLOT_START_HOUR:
			case WID_SCHDISPATCH_ADD_SLOT_STEP_HOUR:
			case WID_SCHDISPATCH_ADD_SLOT_END_HOUR:
				for (uint i = 0; i < 24; i++) {
					SetDParam(0, i);
					d = maxdim(d, GetStringBoundingBox(STR_JUST_INT));
				}
				break;

			case WID_SCHDISPATCH_ADD_SLOT_START_MINUTE:
			case WID_SCHDISPATCH_ADD_SLOT_STEP_MINUTE:
			case WID_SCHDISPATCH_ADD_SLOT_END_MINUTE:
				for (uint i = 0; i < 60; i++) {
					SetDParam(0, i);
					d = maxdim(d, GetStringBoundingBox(STR_JUST_INT));
				}
				break;
		}

		d.width += padding.width;
		d.height += padding.height;
		*size = d;
	}

	virtual void SetStringParameters(int widget) const override
	{
		switch (widget) {
			case WID_SCHDISPATCH_ADD_SLOT_START_HOUR:   SetDParam(0, MINUTES_HOUR(start)); break;
			case WID_SCHDISPATCH_ADD_SLOT_START_MINUTE: SetDParam(0, MINUTES_MINUTE(start)); break;
			case WID_SCHDISPATCH_ADD_SLOT_STEP_HOUR:    SetDParam(0, MINUTES_HOUR(step)); break;
			case WID_SCHDISPATCH_ADD_SLOT_STEP_MINUTE:  SetDParam(0, MINUTES_MINUTE(step)); break;
			case WID_SCHDISPATCH_ADD_SLOT_END_HOUR:     SetDParam(0, MINUTES_HOUR(end)); break;
			case WID_SCHDISPATCH_ADD_SLOT_END_MINUTE:   SetDParam(0, MINUTES_MINUTE(end)); break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count) override
	{
		auto handle_hours_dropdown = [&](uint current) {
			DropDownList list;
			for (uint i = 0; i < 24; i++) {
				DropDownListParamStringItem *item = new DropDownListParamStringItem(STR_JUST_INT, i, false);
				item->SetParam(0, i);
				list.emplace_back(item);
			}
			ShowDropDownList(this, std::move(list), MINUTES_HOUR(current), widget);
		};

		auto handle_minutes_dropdown = [&](uint current) {
			DropDownList list;
			for (uint i = 0; i < 60; i++) {
				DropDownListParamStringItem *item = new DropDownListParamStringItem(STR_JUST_INT, i, false);
				item->SetParam(0, i);
				list.emplace_back(item);
			}
			ShowDropDownList(this, std::move(list), MINUTES_MINUTE(current), widget);
		};

		switch (widget) {
			case WID_SCHDISPATCH_ADD_SLOT_START_HOUR:
				handle_hours_dropdown(this->start);
				break;
			case WID_SCHDISPATCH_ADD_SLOT_START_MINUTE:
				handle_minutes_dropdown(this->start);
				break;
			case WID_SCHDISPATCH_ADD_SLOT_STEP_HOUR:
				handle_hours_dropdown(this->step);
				break;
			case WID_SCHDISPATCH_ADD_SLOT_STEP_MINUTE:
				handle_minutes_dropdown(this->step);
				break;
			case WID_SCHDISPATCH_ADD_SLOT_END_HOUR:
				handle_hours_dropdown(this->end);
				break;
			case WID_SCHDISPATCH_ADD_SLOT_END_MINUTE:
				handle_minutes_dropdown(this->end);
				break;

			case WID_SCHDISPATCH_ADD_SLOT_ADD_BUTTON:
				static_cast<SchdispatchWindow *>(this->parent)->AddMultipleDepartureSlots(this->start, this->step, this->end);
				delete this;
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index) override
	{
		switch (widget) {
			case WID_SCHDISPATCH_ADD_SLOT_START_HOUR:
				this->start = MINUTES_DATE(0, index, MINUTES_MINUTE(this->start));
				break;
			case WID_SCHDISPATCH_ADD_SLOT_START_MINUTE:
				this->start = MINUTES_DATE(0, MINUTES_HOUR(this->start), index);
				break;
			case WID_SCHDISPATCH_ADD_SLOT_STEP_HOUR:
				this->step = MINUTES_DATE(0, index, MINUTES_MINUTE(this->step));
				break;
			case WID_SCHDISPATCH_ADD_SLOT_STEP_MINUTE:
				this->step = MINUTES_DATE(0, MINUTES_HOUR(this->step), index);
				break;
			case WID_SCHDISPATCH_ADD_SLOT_END_HOUR:
				this->end = MINUTES_DATE(0, index, MINUTES_MINUTE(this->end));
				break;
			case WID_SCHDISPATCH_ADD_SLOT_END_MINUTE:
				this->end = MINUTES_DATE(0, MINUTES_HOUR(this->end), index);
				break;
		}


		this->SetWidgetDirty(widget);
	}
};

static const NWidgetPart _nested_scheduled_dispatch_add_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_TIME_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(6, 6, 6),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(6, 6, 6),
				NWidget(WWT_TEXT, COLOUR_BROWN, WID_SCHDISPATCH_ADD_SLOT_START_TEXT), SetDataTip(STR_SCHDISPATCH_ADD_DEPARTURE_SLOTS_START, STR_NULL),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SCHDISPATCH_ADD_SLOT_START_HOUR), SetFill(1, 0), SetDataTip(STR_JUST_INT, STR_DATE_MINUTES_HOUR_TOOLTIP),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SCHDISPATCH_ADD_SLOT_START_MINUTE), SetFill(1, 0), SetDataTip(STR_JUST_INT, STR_DATE_MINUTES_MINUTE_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(6, 6, 6),
				NWidget(WWT_TEXT, COLOUR_BROWN, WID_SCHDISPATCH_ADD_SLOT_STEP_TEXT), SetDataTip(STR_SCHDISPATCH_ADD_DEPARTURE_SLOTS_STEP, STR_NULL),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SCHDISPATCH_ADD_SLOT_STEP_HOUR), SetFill(1, 0), SetDataTip(STR_JUST_INT, STR_DATE_MINUTES_HOUR_TOOLTIP),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SCHDISPATCH_ADD_SLOT_STEP_MINUTE), SetFill(1, 0), SetDataTip(STR_JUST_INT, STR_DATE_MINUTES_MINUTE_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(6, 6, 6),
				NWidget(WWT_TEXT, COLOUR_BROWN, WID_SCHDISPATCH_ADD_SLOT_END_TEXT), SetDataTip(STR_SCHDISPATCH_ADD_DEPARTURE_SLOTS_END, STR_NULL),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SCHDISPATCH_ADD_SLOT_END_HOUR), SetFill(1, 0), SetDataTip(STR_JUST_INT, STR_DATE_MINUTES_HOUR_TOOLTIP),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SCHDISPATCH_ADD_SLOT_END_MINUTE), SetFill(1, 0), SetDataTip(STR_JUST_INT, STR_DATE_MINUTES_MINUTE_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SCHDISPATCH_ADD_SLOT_ADD_BUTTON), SetMinimalSize(100, 12), SetDataTip(STR_SCHDISPATCH_ADD, STR_SCHDISPATCH_ADD_TOOLTIP),
				NWidget(NWID_SPACER), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer()
};

static WindowDesc _scheduled_dispatch_add_desc(
	WDP_CENTER, nullptr, 0, 0,
	WC_SET_DATE, WC_NONE,
	0,
	_nested_scheduled_dispatch_add_widgets, lengthof(_nested_scheduled_dispatch_add_widgets)
);

void ShowScheduledDispatchAddSlotsWindow(SchdispatchWindow *parent, int window_number)
{
	DeleteWindowByClass(WC_SET_DATE);

	new ScheduledDispatchAddSlotsWindow(&_scheduled_dispatch_add_desc, window_number, parent);
}
