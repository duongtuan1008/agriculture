package com.example.myapplication;

public class UserRequest {
    private String action;
    private String email;
    private String password;

    public UserRequest(String action, String email, String password) {
        this.action = action;
        this.email = email;
        this.password = password;
    }
}
