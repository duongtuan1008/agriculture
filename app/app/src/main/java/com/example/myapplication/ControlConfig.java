package com.example.myapplication;

import com.google.gson.annotations.SerializedName;


public class ControlConfig {
    public int id;

    @SerializedName("device_id")
    public String device_id;

    public int auto_mode;
    public int manual_override;
    public int pump;

    public int pump_start_hour;
    public int pump_start_minute;
    public int flow_threshold;

    @SerializedName("updated_at")  // ❗ Chính xác như JSON
    public String updated_at;

    public String repeat_days;
    public int is_enabled;
    public String notes;
}


