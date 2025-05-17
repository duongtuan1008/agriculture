package com.example.myapplication;

import retrofit2.Retrofit;
import retrofit2.converter.gson.GsonConverterFactory;

public class RetrofitClientRaspi {
    // Đường dẫn cơ sở cho API
    private static final String BASE_URL = "http://192.168.137.73/api/";

    private static Retrofit retrofit = null;

    // Phương thức tĩnh để lấy giá trị BASE_URL
    public static String getBaseUrl() {
        return BASE_URL;
    }

    // Phương thức để lấy Retrofit client
    public static Retrofit getClient() {
        if (retrofit == null) {
            retrofit = new Retrofit.Builder()
                    .baseUrl(BASE_URL)  // Cấu hình baseUrl cho Retrofit
                    .addConverterFactory(GsonConverterFactory.create()) // Chuyển đổi JSON thành đối tượng Java
                    .build();
        }
        return retrofit;
    }
}
