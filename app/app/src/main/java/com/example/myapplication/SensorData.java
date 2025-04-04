package com.example.myapplication;

import com.google.gson.annotations.SerializedName;

public class SensorData {
    @SerializedName("id")
    private String id;

    @SerializedName("temperature")
    private String temperature;

    @SerializedName("humidity")
    private String humidity;

    @SerializedName("flame")
    private String flame;

    @SerializedName("light")
    private String light;

    @SerializedName("flow")
    private String flow;

    @SerializedName("soil")
    private String soil;

    @SerializedName("rain")
    private String rain;

    @SerializedName("reading_time")
    private String readingTime;

    // Getters
    public String getId() {
        return id;
    }

    public String getTemperature() {
        return temperature;
    }

    public String getHumidity() {
        return humidity;
    }

    public String getFlame() {
        return flame;
    }

    public String getLight() {
        return light;
    }

    public String getFlow() {
        return flow;
    }

    public String getSoil() {
        return soil;
    }

    public String getRain() {
        return rain;
    }

    public String getReadingTime() {
        return readingTime;
    }
}
