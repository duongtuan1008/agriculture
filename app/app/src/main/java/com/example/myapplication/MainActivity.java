package com.example.myapplication;

import android.os.Bundle;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import android.util.Log;

import com.google.android.material.appbar.MaterialToolbar;

import java.util.List;
import android.graphics.Color;
import retrofit2.Call;
import retrofit2.Callback;
import retrofit2.Response;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Build;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;


import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.appcompat.widget.SwitchCompat;
import android.widget.Toast;
import android.widget.TextView;
import android.widget.CompoundButton;
import android.widget.ImageView;
import androidx.drawerlayout.widget.DrawerLayout;
import com.google.android.material.navigation.NavigationView;
import androidx.appcompat.app.ActionBarDrawerToggle;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.GravityCompat;
import androidx.annotation.NonNull;
import okhttp3.ResponseBody;
import java.io.IOException;
import com.google.gson.Gson;



public class MainActivity extends AppCompatActivity {

    TextView txtTemp, txtHumidity, txtLight, txtSoil,txtFlame ;
    private TextView txtCoverStatus;
    private SwitchCompat switchCover,switchPump, switchLed;
    ImageView iconPump, iconLed;
    private DrawerLayout drawerLayout;
    private ActionBarDrawerToggle toggle;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);
        NavigationView navigationView = findViewById(R.id.navigation_view);

        APIService api = RetrofitClientRaspi.getClient().create(APIService.class);
        // Ánh xạ View
        MaterialToolbar toolbar = findViewById(R.id.toolbar);
        drawerLayout = findViewById(R.id.drawer_layout);
        txtTemp = findViewById(R.id.text_temperature);
        txtHumidity = findViewById(R.id.text_humidity);
        txtLight = findViewById(R.id.text_light);
        txtSoil = findViewById(R.id.text_soil);
        txtFlame = findViewById(R.id.txtFlame);
        txtCoverStatus = findViewById(R.id.txtCoverStatus);
        switchCover = findViewById(R.id.switchCover); // ✅ Sửa lỗi null
        iconPump = findViewById(R.id.iconPump);
        iconLed = findViewById(R.id.iconLed);
        switchPump = findViewById(R.id.switchPump);
        switchLed = findViewById(R.id.switchLed);

        drawerLayout = findViewById(R.id.drawer_layout);
        setSupportActionBar(toolbar);

        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle("Hachi");
        }

// Thêm sự kiện mở drawer khi click vào nút trên Toolbar
        toolbar.setNavigationOnClickListener(v -> {
            drawerLayout.openDrawer(GravityCompat.END);
        });



        // Trạng thái ban đầu của màn che
        txtCoverStatus.setText("🛡️ Màn che: Đóng");
        switchCover.setChecked(true);
        setSupportActionBar(toolbar);
        // Xử lý khi chọn menu trong nav drawer
        navigationView.setNavigationItemSelectedListener(item -> {
            int id = item.getItemId();
            if (id == R.id.nav_home) {
                Toast.makeText(this, "Trang chủ", Toast.LENGTH_SHORT).show();
            } else if (id == R.id.nav_pump) {
                Intent intent = new Intent(MainActivity.this, PumpActivity.class);
                startActivity(intent);
            }
            else if (id == R.id.nav_door) {
                Intent intent = new Intent(MainActivity.this, DoorActivity.class);
                startActivity(intent);
            }
            else if (id == R.id.nav_logout) {
                // ✅ Đặt lại trạng thái chưa đăng nhập
                getSharedPreferences("UserPrefs", MODE_PRIVATE)
                        .edit()
                        .putBoolean("isLoggedIn", false)
                        .apply();

                // ✅ Chuyển về màn hình đăng nhập
                Intent intent = new Intent(MainActivity.this, LoginActivity.class);
                startActivity(intent);
                finish(); // đóng MainActivity để không quay lại được
            }


            drawerLayout.closeDrawer(GravityCompat.END); // ✅ đúng với menu bên phải
            return true;
        });
        // Lắng nghe bật/tắt màn che
        switchCover.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (isChecked) {
                txtCoverStatus.setText("🛡️ Màn che: Đóng");
                Toast.makeText(this, "Đã đóng màn che", Toast.LENGTH_SHORT).show();
                sendControlCommand("curtain", "ON");  // Gửi lệnh mở màn che (ON)
            } else {
                txtCoverStatus.setText("🛡️ Màn che: Mở");
                Toast.makeText(this, "Đã mở màn che", Toast.LENGTH_SHORT).show();
                sendControlCommand("curtain", "OFF"); // Gửi lệnh đóng màn che (OFF)
            }
        });
        switchPump.setOnCheckedChangeListener((buttonView, isChecked) -> {
            iconPump.setImageResource(isChecked ? R.drawable.ic_pump_on : R.drawable.ic_pump_off);
            sendControlCommand("pump", isChecked ? "ON" : "OFF");
        });

        switchLed.setOnCheckedChangeListener((buttonView, isChecked) -> {
            iconLed.setImageResource(isChecked ? R.drawable.ic_light_on : R.drawable.ic_light_off);
            sendControlCommand("led", isChecked ? "ON" : "OFF");
        });


        // Toolbar
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle("Hachi");
        }

        // Tránh che UI bởi thanh trạng thái
        ViewCompat.setOnApplyWindowInsetsListener(toolbar, (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(0, systemBars.top, 0, 0);
            return insets;
        });

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        // Gọi API
        // ⚡ Tự động cập nhật dữ liệu cảm biến mỗi 5 giây
        new android.os.Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                fetchSensorData(); // Gọi lại API lấy dữ liệu mới
                new android.os.Handler().postDelayed(this, 5000); // Lặp lại sau 5s
                fetchDeviceStates();
            }
        }, 5000); // Đợi 5s trước lần đầu

        // sau khi POST xong

    }
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.top_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == R.id.menu_drawer) {
            drawerLayout.openDrawer(GravityCompat.END); // ✅ Mở menu bên phải
            return true;
        }
        return super.onOptionsItemSelected(item);
    }


    private void sendControlCommand(String device, String command) {
        ControlCommand controlCommand = new ControlCommand(device, command);
        APIService api = RetrofitClientRaspi.getClient().create(APIService.class);
        Call<ResponseBody> call = api.sendControl(controlCommand);

        call.enqueue(new Callback<ResponseBody>() {
            @Override
            public void onResponse(Call<ResponseBody> call, Response<ResponseBody> response) {
                Log.d("API_DEBUG", "➡️ URL: " + call.request().url());
                Log.d("API_DEBUG", "📦 Status: " + response.code());

                if (response.isSuccessful() && response.body() != null) {
                    try {
                        String json = response.body().string();
                        Log.d("API", "✅ Server: " + json);
                        fetchDeviceStates(); // Cập nhật trạng thái lại
                    } catch (IOException e) {
                        Log.e("API_ERROR", "❌ Lỗi khi đọc phản hồi: " + e.getMessage());
                    }
                } else {
                    try {
                        Log.e("API_ERROR", "❌ Gửi lệnh lỗi: " +
                                (response.errorBody() != null ? response.errorBody().string() : "null"));
                    } catch (IOException e) {
                        Log.e("API_ERROR", "❌ Lỗi khi đọc errorBody: " + e.getMessage());
                    }
                }
            }

            @Override
            public void onFailure(Call<ResponseBody> call, Throwable t) {
                Log.e("API_ERROR", "❌ Lỗi kết nối: " + t.getMessage(), t);
                Log.e("API_ERROR", "❌ URL: " + call.request().url());
            }
        });
    }

    private void updateDeviceStatus(String deviceId, String status) {
        // Cập nhật trạng thái của các switch theo từng thiết bị
        switch (deviceId) {
            case "pump":
                switchPump.setChecked("ON".equals(status));
                break;
            case "led":
                switchLed.setChecked("ON".equals(status));
                break;
            case "curtain":
                switchCover.setChecked("ON".equals(status));
                break;
        }
    }

    private void fetchDeviceStates() {
        APIService api = RetrofitClientRaspi.getClient().create(APIService.class);
        api.getDeviceStates().enqueue(new Callback<DeviceState>() {
            @Override
            public void onResponse(Call<DeviceState> call, Response<DeviceState> response) {
                Log.d("API_DEBUG", "➡️ URL: " + response.raw().request().url());
                Log.d("API_DEBUG", "📦 Status: " + response.code());

                if (response.isSuccessful()) {
                    DeviceState state = response.body();
                    if (state != null) {
                        Log.d("API", "✅ Server: " + new Gson().toJson(state));

                        // Cập nhật trạng thái của các switch dựa trên dữ liệu nhận được
                        // Đồng bộ trạng thái máy bơm
                        if ("ON".equals(state.getPump())) {
                            switchPump.setChecked(true);  // Nếu trạng thái là "ON", set switch bật
                        } else {
                            switchPump.setChecked(false);  // Nếu trạng thái là "OFF", set switch tắt
                        }

                        // Đồng bộ trạng thái đèn LED
                        if ("ON".equals(state.getLed())) {
                            switchLed.setChecked(true);  // Nếu trạng thái là "ON", set switch bật
                        } else {
                            switchLed.setChecked(false);  // Nếu trạng thái là "OFF", set switch tắt
                        }

                        // Đồng bộ trạng thái màn che (nếu có)

                    } else {
                        Log.e("API_ERROR", "❌ JSON body rỗng (null). Có thể server trả về chuỗi trống.");
                    }
                } else {
                    try {
                        Log.e("API_ERROR", "❌ Lỗi HTTP: " + response.code() + " - " + response.errorBody().string());
                    } catch (IOException e) {
                        Log.e("API_ERROR", "❌ Lỗi khi đọc errorBody: " + e.getMessage());
                    }
                }
            }

            @Override
            public void onFailure(Call<DeviceState> call, Throwable t) {
                Log.e("API_ERROR", "❌ Lỗi kết nối: " + t.getMessage());
            }
        });
    }




    private void fetchSensorData() {
        APIService api = RetrofitClientRaspi.getClient().create(APIService.class);
        api.getSensorData().enqueue(new Callback<List<SensorData>>() {
            @Override
            public void onResponse(Call<List<SensorData>> call, Response<List<SensorData>> response) {
                if (response.isSuccessful() && response.body() != null && !response.body().isEmpty()) {
                    SensorData data = response.body().get(0);

                    txtTemp.setText("☁" + data.getTemperature() + "°C");
                    txtHumidity.setText("💧 " + data.getHumidity() + "%");
                    txtLight.setText("🔆 " + data.getLight() + " lux");
                    txtSoil.setText("🌱 " + data.getSoil());
                    txtFlame = findViewById(R.id.txtFlame);  // giả sử bạn có textView này

                    if ("1".equals(data.getFlame())) {
                        txtFlame.setText("🔥 Có lửa!");
                        txtFlame.setTextColor(Color.RED);
                        showSensorAlert("🔥 CẢNH BÁO", "Phát hiện lửa tại cảm biến số 1!");
                        // Cảnh báo người dùng
                        Toast.makeText(MainActivity.this, "⚠️ CẢNH BÁO: Phát hiện lửa!", Toast.LENGTH_LONG).show();

                    } else {
                        txtFlame.setText("✅ An toàn");
                        txtFlame.setTextColor(Color.WHITE);
                    }

                    ;
                } else {
                    Toast.makeText(MainActivity.this, "Không có dữ liệu", Toast.LENGTH_SHORT).show();
                }
            }

            @Override
            public void onFailure(Call<List<SensorData>> call, Throwable t) {
                Log.e("API_ERROR", "Lỗi kết nối: " + t.getMessage(), t);
                Toast.makeText(MainActivity.this, "Lỗi kết nối đến API", Toast.LENGTH_SHORT).show();
            }
        });
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            CharSequence name = "Cảnh báo cảm biến";
            String description = "Thông báo từ hệ thống cảm biến";
            int importance = NotificationManager.IMPORTANCE_DEFAULT;
            NotificationChannel channel = new NotificationChannel("sensor_channel", name, importance);
            channel.setDescription(description);

            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }
    }
    private void showSensorNotification(String message) {
        Intent intent = new Intent(this, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, intent, PendingIntent.FLAG_IMMUTABLE);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, "sensor_channel")
                .setSmallIcon(R.drawable.ic_launcher_foreground) // đổi icon nếu muốn
                .setContentTitle("📡 Cảm biến thông báo")
                .setContentText(message)
                .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                .setContentIntent(pendingIntent)
                .setAutoCancel(true); // tự tắt khi bấm

        NotificationManagerCompat notificationManager = NotificationManagerCompat.from(this);
        notificationManager.notify(1001, builder.build());
    }
    private void showSensorAlert(String title, String message) {
        String CHANNEL_ID = "sensor_alert_channel";

        // Tạo kênh thông báo nếu Android >= 8.0 (API 26)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            CharSequence name = "Cảnh báo cảm biến";
            String description = "Thông báo khi có cảnh báo từ cảm biến";
            int importance = NotificationManager.IMPORTANCE_HIGH;

            NotificationChannel channel = new NotificationChannel(CHANNEL_ID, name, importance);
            channel.setDescription(description);
            channel.enableLights(true);
            channel.setLightColor(Color.RED);
            channel.enableVibration(true);

            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }

        // Intent khi bấm vào thông báo => mở MainActivity
        Intent intent = new Intent(this, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        PendingIntent pendingIntent = PendingIntent.getActivity(
                this, 0, intent,
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? PendingIntent.FLAG_IMMUTABLE : 0
        );

        // Tạo nội dung thông báo
        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_launcher_foreground)
                .setContentTitle(title)
                .setContentText(message)
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setContentIntent(pendingIntent)
                .setAutoCancel(true); // tự tắt khi người dùng bấm

        NotificationManagerCompat notificationManager = NotificationManagerCompat.from(this);
        notificationManager.notify(1001, builder.build());
    }



}
