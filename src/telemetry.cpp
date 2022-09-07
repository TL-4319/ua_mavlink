/*
* Brian R Taylor
* brian.taylor@bolderflight.com
* 
* Copyright (c) 2022 Bolder Flight Systems Inc
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the “Software”), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

#if defined(ARDUINO)
#include "Arduino.h"
#else
#include "core/core.h"
#endif
#include <array>
#include "telemetry.h"  // NOLINT
#include "mavlink/mavlink_types.h"
#include "mavlink/common/mavlink.h"
#include "units.h"  // NOLINT
#include "navigation.h"  // NOLINT

namespace bfs {

void MavLinkTelemetry::Update() {
  /*
  * Data stream periods are set by default to -1 to disable them. Check for
  * enabled streams and, if their timer is greater than the period, send
  * them and reset the timer.
  */
  for (std::size_t stream_num = 0; stream_num < NUM_DATA_STREAMS_;
       stream_num++) {
    if (data_stream_period_ms_[stream_num] > 0) {
      if (data_stream_timer_ms_[stream_num] >
          data_stream_period_ms_[stream_num]) {
        (this->*(streams_[stream_num]))();
        data_stream_timer_ms_[stream_num] = 0;
      }
    }
  }
}
void MavLinkTelemetry::MsgHandler(const mavlink_message_t &ref) {
  switch (ref.msgid) {
    case MAVLINK_MSG_ID_REQUEST_DATA_STREAM: {
      mavlink_msg_request_data_stream_decode(&ref, &request_stream_);
      ParseRequestDataStream(request_stream_);
      break;
    }
  }
}
void MavLinkTelemetry::SRx_ALL() {
  SRx_RAW_SENS();
  SRx_EXT_STAT();
  SRx_RC_CHAN();
  SRx_RAW_CTRL();
  SRx_POSITION();
  SRx_EXTRA1();
  SRx_EXTRA2();
  SRx_EXTRA3();
}
void MavLinkTelemetry::SRx_EXT_STAT() {
  SendSysStatus();
  SendBatteryStatus();
}
void MavLinkTelemetry::SendSysStatus() {
  sensors_present_ = 0;
  if (gyro_installed_) {
    sensors_present_ |= MAV_SYS_STATUS_SENSOR_3D_GYRO;
  }
  if (accel_installed_) {
    sensors_present_ |= MAV_SYS_STATUS_SENSOR_3D_ACCEL;
  }
  if (mag_installed_) {
    sensors_present_ |= MAV_SYS_STATUS_SENSOR_3D_MAG;
  }
  if (static_pres_installed_) {
    sensors_present_ |= MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE;
  }
  if (diff_pres_installed_) {
    sensors_present_ |= MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE;
  }
  if (gnss_installed_) {
    sensors_present_ |= MAV_SYS_STATUS_SENSOR_GPS;
  }
  if (inceptor_installed_) {
    sensors_present_ |= MAV_SYS_STATUS_SENSOR_RC_RECEIVER;
  }
  /* Check sensor health */
  sensors_healthy_ = 0;
  if (gyro_healthy_) {
    sensors_healthy_ |= MAV_SYS_STATUS_SENSOR_3D_GYRO;
  }
  if (accel_healthy_) {
    sensors_healthy_ |= MAV_SYS_STATUS_SENSOR_3D_ACCEL;
  }
  if (mag_healthy_) {
    sensors_healthy_ |= MAV_SYS_STATUS_SENSOR_3D_MAG;
  }
  if (static_pres_healthy_) {
    sensors_healthy_ |= MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE;
  }
  if (diff_pres_healthy_) {
    sensors_healthy_ |= MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE;
  }
  if (gnss_healthy_) {
    sensors_healthy_ |= MAV_SYS_STATUS_SENSOR_GPS;
  }
  if (inceptor_healthy_) {
    sensors_healthy_ |= MAV_SYS_STATUS_SENSOR_RC_RECEIVER;
  }
  load_ = static_cast<uint16_t>(1000.0f * static_cast<float>(frame_time_us_) /
          static_cast<float>(frame_period_us_));
  if (battery_volt_.set) {
    voltage_battery_ = static_cast<uint16_t>(battery_volt_.val * 1000.0f);
  }
  if (battery_current_ma_.set) {
    current_battery_ = static_cast<int16_t>(battery_current_ma_.val * 0.1f);
  }
  if (battery_remaining_prcnt_.set) {
    battery_remaining_ = static_cast<int8_t>(battery_remaining_prcnt_.val);
  }
  msg_len_ = mavlink_msg_sys_status_pack(sys_id_, comp_id_, &msg_,
                                         sensors_present_, sensors_present_,
                                         sensors_healthy_, load_,
                                         voltage_battery_, current_battery_,
                                         battery_remaining_, drop_rate_comm_,
                                         errors_comm_, errors_count_[0],
                                         errors_count_[1], errors_count_[2],
                                         errors_count_[3]);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SendGpsRawInt() {
  switch (gnss_fix_) {
    case GNSS_FIX_NONE: {
      fix_ = GPS_FIX_TYPE_NO_FIX;
      break;
    }
    case GNSS_FIX_2D: {
      fix_ = GPS_FIX_TYPE_2D_FIX;
      break;
    }
    case GNSS_FIX_3D: {
      fix_ = GPS_FIX_TYPE_3D_FIX;
      break;
    }
    case GNSS_FIX_DGNSS: {
      fix_ = GPS_FIX_TYPE_DGPS;
      break;
    }
    case GNSS_FIX_RTK_FLOAT: {
      fix_ = GPS_FIX_TYPE_RTK_FLOAT;
      break;
    }
    case GNSS_FIX_RTK_FIXED: {
      fix_ = GPS_FIX_TYPE_RTK_FIXED;
      break;
    }
  }
  lat_dege7_ = static_cast<int32_t>(rad2deg(gnss_lat_rad_) * 1e7);
  lon_dege7_ = static_cast<int32_t>(rad2deg(gnss_lon_rad_) * 1e7);
  alt_msl_mm_ = static_cast<int32_t>(gnss_alt_msl_m_ * 1000.0f);
  if (gnss_hdop_.set) {
    eph_ = static_cast<uint16_t>(100.0f * gnss_hdop_.val);
  }
  if (gnss_vdop_.set) {
    epv_ = static_cast<uint16_t>(100.0f * gnss_vdop_.val);
  }
  if (gnss_vel_mps_.set) {
    vel_cmps_ = static_cast<uint16_t>(100.0f * gnss_vel_mps_.val);
  }
  if (gnss_track_rad_.set) {
    track_cdeg_ = static_cast<uint16_t>(100.0f *
                 rad2deg(WrapTo2Pi(gnss_track_rad_.val)));
  }
  if (gnss_num_sv_.set) {
    num_sv_ = gnss_num_sv_.val;
  }
  alt_wgs84_mm_ = static_cast<int32_t>(gnss_alt_wgs84_m_ * 1000.0f);
  h_acc_mm_ = static_cast<uint32_t>(gnss_horz_acc_m_ * 1000.0f);
  v_acc_mm_ = static_cast<uint32_t>(gnss_vert_acc_m_ * 1000.0f);
  vel_acc_mmps_ = static_cast<uint32_t>(gnss_vel_acc_mps_ * 1000.0f);
  hdg_acc_dege5_ = static_cast<uint32_t>(100000.0f *
                                                 rad2deg(gnss_track_acc_rad_));
  msg_len_ = mavlink_msg_gps_raw_int_pack(sys_id_, comp_id_, &msg_,
                                          sys_time_us_, fix_, lat_dege7_,
                                          lon_dege7_, alt_msl_mm_, eph_, epv_,
                                          vel_cmps_, track_cdeg_, num_sv_,
                                          alt_wgs84_mm_, h_acc_mm_, v_acc_mm_,
                                          vel_acc_mmps_, hdg_acc_dege5_,
                                          yaw_cdeg_);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SRx_EXTRA1() {
  SendAttitude();
}
void MavLinkTelemetry::SendAttitude() {
  sys_time_ms_ = static_cast<uint32_t>(sys_time_us_ / 1000);
  if (nav_hdg_rad_.set) {
    yaw_rad_ = WrapToPi(nav_hdg_rad_.val);
  }
  msg_len_ = mavlink_msg_attitude_pack(sys_id_, comp_id_, &msg_,
                                       sys_time_ms_, nav_roll_rad_,
                                       nav_pitch_rad_, yaw_rad_,
                                       nav_gyro_x_radps_, nav_gyro_y_radps_,
                                       nav_gyro_z_radps_);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SRx_EXTRA2() {
  SendVfrHud();
}
void MavLinkTelemetry::SendVfrHud() {
  if (nav_hdg_rad_.set) {
    hdg_deg_ = static_cast<int16_t>(
      rad2deg(WrapTo2Pi(nav_hdg_rad_.val)));
  }
  if (!use_throttle_prcnt_) {
    throttle_ = static_cast<uint16_t>(inceptor_[throttle_ch_] * 100.0f);
  } else {
    throttle_ = throttle_prcnt_;
  }
  climb_mps_ = -1.0f * nav_down_vel_mps_;
  msg_len_ = mavlink_msg_vfr_hud_pack(sys_id_, comp_id_, &msg_,
                                      nav_ias_mps_, nav_gnd_spd_mps_,
                                      hdg_deg_, throttle_, nav_alt_msl_m_,
                                      climb_mps_);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SRx_EXTRA3() {}
void MavLinkTelemetry::SendBatteryStatus() {
  if (battery_volt_.set) {
    volt_[0] = static_cast<uint16_t>(battery_volt_.val * 1000.0f);
  }
  if (battery_current_ma_.set) {
    current_ = static_cast<int16_t>(battery_current_ma_.val * 0.1f);
  }
  if (battery_consumed_mah_.set) {
    current_consumed_ = static_cast<int32_t>(battery_consumed_mah_.val);
  }
  if (battery_remaining_prcnt_.set) {
    battery_remaining_ = static_cast<int8_t>(battery_remaining_prcnt_.val);
  }
  if (battery_remaining_time_s_.set) {
    time_remaining_ = static_cast<int32_t>(battery_remaining_time_s_.val);
  }
  msg_len_ = mavlink_msg_battery_status_pack(sys_id_, comp_id_, &msg_,
                                             id_, battery_function_, type_,
                                             temp_, &volt_[0], current_,
                                             current_consumed_,
                                             energy_consumed_,
                                             battery_remaining_,
                                             time_remaining_,
                                             charge_state_, &volt_[10],
                                             battery_mode_, fault_bitmask_);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SRx_POSITION() {
  SendLocalPositionNed();
  SendGlobalPositionInt();
}
void MavLinkTelemetry::SendLocalPositionNed() {
  sys_time_ms_ = static_cast<uint32_t>(sys_time_us_ / 1000);
  msg_len_ = mavlink_msg_local_position_ned_pack(sys_id_, comp_id_, &msg_,
                                                 sys_time_ms_,
                                                 nav_north_pos_m_,
                                                 nav_east_pos_m_,
                                                 nav_down_pos_m_,
                                                 nav_north_vel_mps_,
                                                 nav_east_vel_mps_,
                                                 nav_down_vel_mps_);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SendGlobalPositionInt() {
  sys_time_ms_ = static_cast<uint32_t>(sys_time_us_ / 1000);
  lat_dege7_ = static_cast<int32_t>(rad2deg(nav_lat_rad_) * 1e7);
  lon_dege7_ = static_cast<int32_t>(rad2deg(nav_lon_rad_) * 1e7);
  alt_msl_mm_ = static_cast<int32_t>(nav_alt_msl_m_ * 1000.0f);
  alt_agl_mm_ = static_cast<int32_t>(nav_alt_agl_m_ * 1000.0f);
  vx_cmps_ = static_cast<int16_t>(nav_north_vel_mps_ * 100.0f);
  vy_cmps_ = static_cast<int16_t>(nav_east_vel_mps_ * 100.0f);
  vz_cmps_ = static_cast<int16_t>(nav_down_vel_mps_ * 100.0f);
  if (nav_hdg_rad_.set) {
    hdg_cdeg_ = static_cast<uint16_t>(100.0f *
      rad2deg(WrapTo2Pi(nav_hdg_rad_.val)));
  }
  msg_len_ = mavlink_msg_global_position_int_pack(sys_id_, comp_id_, &msg_,
                                                  sys_time_ms_, lat_dege7_,
                                                  lon_dege7_, alt_msl_mm_,
                                                  alt_agl_mm_, vx_cmps_,
                                                  vy_cmps_, vz_cmps_,
                                                  hdg_cdeg_);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SRx_RAW_SENS() {
  SendScaledImu();
  SendGpsRawInt();
  SendScaledPressure();
}
void MavLinkTelemetry::SendScaledImu() {
  sys_time_ms_ = static_cast<uint32_t>(sys_time_us_ / 1000);
  accel_x_mg_ = static_cast<int16_t>(convacc(imu_accel_x_mps2_,
                LinAccUnit::MPS2, LinAccUnit::G) * 1000.0f);
  accel_y_mg_ = static_cast<int16_t>(convacc(imu_accel_y_mps2_,
                LinAccUnit::MPS2, LinAccUnit::G) * 1000.0f);
  accel_z_mg_ = static_cast<int16_t>(convacc(imu_accel_z_mps2_,
                LinAccUnit::MPS2, LinAccUnit::G) * 1000.0f);
  gyro_x_mradps_ = static_cast<int16_t>(imu_gyro_x_radps_ * 1000.0f);
  gyro_y_mradps_ = static_cast<int16_t>(imu_gyro_y_radps_ * 1000.0f);
  gyro_z_mradps_ = static_cast<int16_t>(imu_gyro_z_radps_ * 1000.0f);
  mag_x_mgauss_ = static_cast<int16_t>(imu_mag_x_ut_ * 10.0f);
  mag_y_mgauss_ = static_cast<int16_t>(imu_mag_y_ut_ * 10.0f);
  mag_z_mgauss_ = static_cast<int16_t>(imu_mag_z_ut_ * 10.0f);
  if (imu_die_temp_c_.set) {
    temp_cc_ = static_cast<int16_t>(imu_die_temp_c_.val * 100.0f);
    if (temp_cc_ == 0) {
      temp_cc_ = 1;
    }
  }
  msg_len_ = mavlink_msg_scaled_imu_pack(sys_id_, comp_id_, &msg_,
                                         sys_time_ms_,
                                         accel_x_mg_, accel_y_mg_, accel_z_mg_,
                                         gyro_x_mradps_, gyro_y_mradps_,
                                         gyro_z_mradps_, mag_x_mgauss_,
                                         mag_y_mgauss_, mag_z_mgauss_,
                                         temp_cc_);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SendScaledPressure() {
  sys_time_ms_ = static_cast<uint32_t>(sys_time_us_ / 1000);
  static_pres_hpa_ = static_pres_pa_ / 100.0f;
  diff_pres_hpa_ = diff_pres_pa_ / 100.0f;
  static_temp_cc_ =
    static_cast<int16_t>(static_pres_die_temp_c_ * 100.0f);
  if (diff_pres_die_temp_c_.set) {
    diff_temp_cc_ = static_cast<int16_t>(diff_pres_die_temp_c_.val * 100.0f);
    if (diff_temp_cc_ == 0) {
      diff_temp_cc_ = 1;
    }
  }
  msg_len_ = mavlink_msg_scaled_pressure_pack(sys_id_, comp_id_, &msg_,
                                              sys_time_ms_,
                                              static_pres_hpa_,
                                              diff_pres_hpa_,
                                              static_temp_cc_,
                                              diff_temp_cc_);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SRx_RC_CHAN() {
  SendServoOutputRaw();
  SendRcChannels();
}
void MavLinkTelemetry::SendServoOutputRaw() {
  /* Transform percent [0 - 1] to PWM value */
  if (!use_raw_effector_) {
    for (std::size_t i = 0; i < 16; i++) {
      servo_raw_[i] = static_cast<uint16_t>(effector_[i] * 1000.0f + 1000.0f);
    }
  } else {
    for (std::size_t i = 0; i < 16; i++) {
      servo_raw_[i] = effector_[i];
    }
  }
  msg_len_ = mavlink_msg_servo_output_raw_pack(sys_id_, comp_id_, &msg_,
                                               sys_time_us_, port_,
                                               servo_raw_[0], servo_raw_[1],
                                               servo_raw_[2], servo_raw_[3],
                                               servo_raw_[4], servo_raw_[5],
                                               servo_raw_[6], servo_raw_[7],
                                               servo_raw_[8], servo_raw_[9],
                                               servo_raw_[10], servo_raw_[11],
                                               servo_raw_[12], servo_raw_[13],
                                               servo_raw_[14], servo_raw_[15]);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SendRcChannels() {
  sys_time_ms_ = static_cast<uint32_t>(sys_time_us_ / 1000);
  /* Transform percent [0 - 1] to PWM value */
  if (!use_raw_inceptor_) {
    for (std::size_t i = 0; i < chancount_; i++) {
      chan_[i] = static_cast<uint16_t>(inceptor_[i] * 1000.0f + 1000.0f);
    }
  } else {
    for (std::size_t i = 0; i < chancount_; i++) {
      chan_[i] = inceptor_[i];
    }
  }
  msg_len_ = mavlink_msg_rc_channels_pack(sys_id_, comp_id_, &msg_,
                                          sys_time_ms_, chancount_,
                                          chan_[0], chan_[1],
                                          chan_[2], chan_[3],
                                          chan_[4], chan_[5],
                                          chan_[6], chan_[7],
                                          chan_[8], chan_[9],
                                          chan_[10], chan_[11],
                                          chan_[12], chan_[13],
                                          chan_[14], chan_[15],
                                          chan_[16], chan_[17], rssi_);
  mavlink_msg_to_send_buffer(msg_buf_, &msg_);
  bus_->write(msg_buf_, msg_len_);
}
void MavLinkTelemetry::SRx_RAW_CTRL() {}
void MavLinkTelemetry::SRx_EMPTY() {}
void MavLinkTelemetry::ParseRequestDataStream(
                       const mavlink_request_data_stream_t &ref) {
  if ((ref.target_system == sys_id_) &&
      (ref.target_component == comp_id_)) {
    if (ref.start_stop) {
      data_stream_period_ms_[ref.req_stream_id] =
        static_cast<int32_t>(1000.0f /
        static_cast<float>(ref.req_message_rate));
    } else {
      data_stream_period_ms_[ref.req_stream_id] = -1;
    }
  }
}

}  // namespace bfs
