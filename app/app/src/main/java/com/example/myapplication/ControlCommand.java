package com.example.myapplication;

public class ControlCommand {
    private String device;
    private String state;

    // Constructor, getters, and setters
    public ControlCommand(String device, String state) {
        this.device = device;
        this.state = state;
    }

    public String getDevice() {
        return device;
    }

    public void setDevice(String device) {
        this.device = device;
    }

    public String getState() {
        return state;
    }

    public void setState(String state) {
        this.state = state;
    }
}

