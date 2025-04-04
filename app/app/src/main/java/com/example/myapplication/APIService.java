package com.example.myapplication;

import java.util.List;

import retrofit2.Call;
import retrofit2.http.GET;
import retrofit2.http.POST;
import retrofit2.http.FormUrlEncoded;
import retrofit2.http.Field;
import retrofit2.http.Query;

import okhttp3.ResponseBody;

public interface APIService {

    // Lấy dữ liệu cảm biến (nếu bạn có API get-data.php)
    @GET("get-data.php")
    Call<List<SensorData>> getSensorData();

    // Lấy danh sách lịch tưới
    @GET("control.php")
    Call<List<ControlConfig>> getAllConfigs(@Query("esp") int esp);

    @GET("control.php")
    Call<ControlResponse> getConfig(@Query("device_id") String deviceId);


    @GET("control.php")
    Call<ResponseBody> deleteSchedule(@Query("action") String action, @Query("id") int id);

    // Thêm lịch tưới mới
    @FormUrlEncoded
    @POST("watering_schedule.php?action=edit")
    Call<ResponseBody> editSchedule(
            @Field("id") int id,
            @Field("start_hour") int hour,
            @Field("start_minute") int minute,
            @Field("duration") int duration,
            @Field("repeat_days") String repeatDays
    );
    // Lấy trạng thái: {"pump":"ON", "led":"OFF"}
    @GET("pump-command.php")
    Call<DeviceState> getDeviceStates();

    // Gửi điều khiển: {"status":"ok", "message":"..."}
    @FormUrlEncoded
    @POST("pump-command.php")
    Call<ResponseBody> sendControl(
            @Field("device") String device,
            @Field("state") String state
    );

}
