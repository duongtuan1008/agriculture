package com.example.myapplication;

import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.*;
import androidx.appcompat.app.AppCompatActivity;

import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Calendar;
import okhttp3.ResponseBody;
import android.view.Gravity;
import android.graphics.Color;
import java.util.List;
import retrofit2.Call;
import retrofit2.Callback;
import retrofit2.Response;
import java.net.URLEncoder;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import org.json.JSONObject;
import android.graphics.Typeface;
import org.json.JSONArray;
import org.json.JSONObject;
import com.google.gson.Gson;
import java.io.IOException;



public class PumpActivity extends AppCompatActivity {
    Switch switchAuto;
    SeekBar seekThreshold;
    TextView txtThreshold, txtScheduleLabel;
    TimePicker timePicker;
    Button btnSave,btnCancelEdit;
    LinearLayout layoutDays, layoutSchedules;
    EditText editThreshold;
    private int editingScheduleId = -1; // -1 nghĩa là đang thêm mới
    ArrayList<TextView> dayViews = new ArrayList<>();
    int threshold = 3000;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_pump);
        editThreshold = findViewById(R.id.editThreshold);

        switchAuto = findViewById(R.id.switchAuto);
        seekThreshold = findViewById(R.id.seekThreshold);
        txtThreshold = findViewById(R.id.txtThreshold);
        timePicker = findViewById(R.id.timePicker);
        editThreshold.setText(String.valueOf(threshold));

        btnCancelEdit = findViewById(R.id.btnCancelEdit);
        btnSave = findViewById(R.id.btnSave);
        layoutDays = findViewById(R.id.layoutDays);
        layoutSchedules = findViewById(R.id.layoutSchedules);
        txtScheduleLabel = findViewById(R.id.txtScheduleLabel);
        // Cập nhật hiển thị ngưỡng độ ẩm khi kéo SeekBar
        layoutSchedules = findViewById(R.id.layoutSchedules);
        seekThreshold.setProgress(threshold);
        txtThreshold.setText("Lượng nước cần tưới (mL): " + threshold);
        seekThreshold.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                threshold = progress;
                txtThreshold.setText("Lượng nước cần tưới (mL): " + threshold);
            }

            public void onStartTrackingTouch(SeekBar seekBar) {}
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // Tạo các nút chọn ngày trong tuần
        String[] dayLabels = {"2", "3", "4", "5", "6", "7", "CN"};
        for (String day : dayLabels) {
            TextView tv = new TextView(this);
            tv.setText(day);
            tv.setPadding(16, 8, 16, 8);
            tv.setBackgroundColor(getResources().getColor(android.R.color.darker_gray));
            tv.setTextColor(getResources().getColor(android.R.color.black));
            tv.setClickable(true);
            tv.setOnClickListener(v -> {
                v.setSelected(!v.isSelected());
                v.setBackgroundColor(v.isSelected() ? 0xFF4CAF50 : 0xFFAAAAAA); // Xanh nếu được chọn
            });

            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
            );
            params.setMargins(8, 0, 8, 0);
            layoutDays.addView(tv, params);
            dayViews.add(tv);
        }

        btnSave.setOnClickListener(v -> {
            boolean auto = switchAuto.isChecked();
            int hour = timePicker.getHour();
            int minute = timePicker.getMinute();

            String thresholdStr = editThreshold.getText().toString().trim();
            if (thresholdStr.isEmpty()) {
                Toast.makeText(this, "Vui lòng nhập Lượng nước cần tưới", Toast.LENGTH_SHORT).show();
                return;
            }
            int threshold = Integer.parseInt(thresholdStr);

            // ✅ Sửa đúng cách lấy ngày theo chuẩn "2,3,4,5,6,7,CN"
            String[] dayKeys = {"2", "3", "4", "5", "6", "7", "CN"}; // mapping chính xác thứ trong tuần
            StringBuilder selectedDays = new StringBuilder();

            for (int i = 0; i < dayViews.size(); i++) {
                TextView tv = dayViews.get(i);
                if (tv.isSelected()) {
                    selectedDays.append(dayKeys[i]).append(",");
                }
            }

            String repeatDays = selectedDays.toString().replaceAll(",$", "");

            if (repeatDays.isEmpty()) {
                Toast.makeText(this, "Vui lòng chọn ít nhất một ngày tưới", Toast.LENGTH_SHORT).show();
                return;
            }

            // 🚀 Gửi đến control.php (thêm hoặc sửa)
            sendToServerWithId(
                    editingScheduleId,
                    auto,
                    threshold,
                    hour,
                    minute,
                    repeatDays
            );
        });

        loadConfigFromServer();
        fetchSchedules();
    }

    private void sendToServer(boolean auto, int threshold, int hour, int minute, int duration, String repeatDays) {
        new Thread(() -> {
            try {
                URL url = new URL("http://192.168.137.74/api/control.php"); // ← Đảm bảo IP đúng
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("POST");
                conn.setDoOutput(true);
                conn.setRequestProperty("Content-Type", "application/x-www-form-urlencoded");

                // ✅ Thêm repeat_days vào postData
                String postData =
                        "auto_mode=" + (auto ? "1" : "0") +
                                "&manual_override=0" +
                                "&pump=0" +
                                "&soil_threshold=" + threshold +
                                "&pump_start_hour=" + hour +
                                "&pump_start_minute=" + minute +
                                "&repeat_days=" + URLEncoder.encode(repeatDays, "UTF-8");;

                OutputStream os = conn.getOutputStream();
                os.write(postData.getBytes());
                os.flush();
                os.close();

                int responseCode = conn.getResponseCode();
                Log.d("HTTP", "Server response: " + responseCode);

                conn.disconnect();
            } catch (Exception e) {
                Log.e("HTTP", "❌ Lỗi gửi dữ liệu: " + e.getMessage());
            }
        }).start();
    }
    private void sendToServerWithId(int id, boolean auto, int threshold, int hour, int minute, String repeatDays) {
        new Thread(() -> {
            try {
                URL url = new URL("http://192.168.137.74/api/control.php");
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("POST");
                conn.setDoOutput(true);
                conn.setRequestProperty("Content-Type", "application/x-www-form-urlencoded");

                String postData =
                        (id > 0 ? "id=" + id + "&" : "") +
                                "auto_mode=" + (auto ? "1" : "0") +
                                "&manual_override=0" +
                                "&pump=0" +
                                "&flow_threshold=" + threshold +
                                "&pump_start_hour=" + hour +
                                "&pump_start_minute=" + minute +
                                "&repeat_days=" + URLEncoder.encode(repeatDays, "UTF-8") +
                                "&is_enabled=1";

                OutputStream os = conn.getOutputStream();
                os.write(postData.getBytes());
                os.flush();
                os.close();

                conn.getResponseCode();
                editingScheduleId = -1;

                runOnUiThread(() -> fetchSchedules());
                conn.disconnect();
            } catch (Exception e) {
                Log.e("HTTP", "❌ Lỗi gửi dữ liệu: " + e.getMessage());
            }
        }).start();
    }

    private void fetchSchedules() {
        layoutSchedules.removeAllViews(); // 🔄 Xoá lịch cũ trên giao diện

        APIService api = RetrofitClientRaspi.getClient().create(APIService.class);
        api.getAllConfigs(1).enqueue(new Callback<List<ControlConfig>>() {
            @Override
            public void onResponse(Call<List<ControlConfig>> call, Response<List<ControlConfig>> response) {
                Log.d("LỊCH_DEBUG", "Code phản hồi: " + response.code());

                if (response.isSuccessful() && response.body() != null) {
                    List<ControlConfig> configs = response.body();
                    Log.d("LỊCH", "📥 Nhận được " + configs.size() + " lịch tưới");
                    Log.d("LỊCH_RAW", new Gson().toJson(configs)); // In toàn bộ JSON đã parse

                    if (!configs.isEmpty()) {
                        txtScheduleLabel.setVisibility(View.VISIBLE);
                        layoutSchedules.setVisibility(View.VISIBLE);
                    } else {
                        txtScheduleLabel.setVisibility(View.GONE);
                        layoutSchedules.setVisibility(View.GONE);
                    }

                    for (ControlConfig config : configs) {
                        addScheduleItem(
                                config.id,
                                config.is_enabled == 1,
                                config.pump_start_hour,
                                config.pump_start_minute,
                                config.repeat_days,
                                config.flow_threshold
                        );
                    }
                } else {
                    // Phản hồi lỗi – có thể do thiếu field, JSON sai, v.v.
                    Log.e("LỊCH", "⚠️ Phản hồi không hợp lệ từ server");

                    // Nếu có errorBody → in lỗi JSON
                    try {
                        if (response.errorBody() != null) {
                            Log.e("LỊCH_ERROR", "❌ Lỗi: " + response.errorBody().string());
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }

                    Toast.makeText(PumpActivity.this, "Không nhận được dữ liệu lịch", Toast.LENGTH_SHORT).show();
                }
            }

            @Override
            public void onFailure(Call<List<ControlConfig>> call, Throwable t) {
                Log.e("LỊCH", "❌ Lỗi khi gọi API: " + t.getMessage());
                Toast.makeText(PumpActivity.this, "Lỗi khi tải lịch tưới", Toast.LENGTH_SHORT).show();
            }
        });
    }



    private void loadConfigFromServer() {
        new Thread(() -> {
            try {
                URL url = new URL("http://192.168.137.74/api/control.php?esp=1");
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("GET");

                BufferedReader in = new BufferedReader(new InputStreamReader(conn.getInputStream()));
                StringBuilder builder = new StringBuilder(); // ✅ tên mới tránh trùng
                String line;

                while ((line = in.readLine()) != null) {
                    builder.append(line);
                }

                in.close();
                conn.disconnect();

                String jsonText = builder.toString(); // ✅ chuỗi JSON
                JSONArray jsonArray = new JSONArray(jsonText); // ✅ xử lý mảng JSON

                if (jsonArray.length() > 0) {
                    JSONObject json = jsonArray.getJSONObject(0); // ✅ lấy lịch đầu tiên

                    int hour = json.getInt("pump_start_hour");
                    int minute = json.getInt("pump_start_minute");
                    int threshold = json.getInt("flow_threshold");
                    int autoMode = json.getInt("auto_mode");
                    String repeatDays = json.optString("repeat_days", "");

                    runOnUiThread(() -> {
                        try {
                            // Set vào giao diện
                            timePicker.setHour(hour);
                            timePicker.setMinute(minute);
                            editThreshold.setText(String.valueOf(threshold));
                            switchAuto.setChecked(autoMode == 1);

                            for (TextView tv : dayViews) {
                                String day = tv.getText().toString().trim();
                                String[] repeatDayArray = repeatDays.split(",");
                                boolean isSelected = false;
                                for (String d : repeatDayArray) {
                                    if (d.trim().equals(day)) {
                                        isSelected = true;
                                        break;
                                    }
                                }
                                tv.setSelected(isSelected);
                                tv.setBackgroundColor(isSelected
                                        ? getResources().getColor(R.color.green)
                                        : getResources().getColor(R.color.gray));

                            }

                            Log.d("LOAD", "✅ Đã set xong cấu hình từ server");

                        } catch (Exception e) {
                            Log.e("LOAD", "❌ Parse JSON lỗi: " + e.getMessage());
                        }
                    });
                }

            } catch (Exception e) {
                Log.e("LOAD", "❌ Lỗi kết nối hoặc xử lý JSON: " + e.getMessage());
            }
        }).start();
    }

    private void addScheduleItem(int id, boolean enabled, int hour, int minute, String repeatDays, int threshold) {
        LinearLayout item = new LinearLayout(this);
        item.setOrientation(LinearLayout.VERTICAL);
        item.setPadding(24, 24, 24, 24);
        item.setBackgroundResource(R.drawable.bg_schedule_box);

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        params.setMargins(0, 0, 0, 24);
        item.setLayoutParams(params);

        // Hàng chứa thời gian + các nút
        LinearLayout timeRow = new LinearLayout(this);
        timeRow.setOrientation(LinearLayout.HORIZONTAL);
        timeRow.setGravity(Gravity.CENTER_VERTICAL);

        TextView txtTime = new TextView(this);
        txtTime.setText(String.format("%02d:%02d", hour, minute));
        txtTime.setTextSize(24);
        txtTime.setTextColor(Color.BLACK);
        txtTime.setTypeface(null, Typeface.BOLD);
        txtTime.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));

        // Icon trạng thái bật/tắt
        ImageView icon = new ImageView(this);
        icon.setImageResource(enabled
                ? android.R.drawable.button_onoff_indicator_on
                : android.R.drawable.button_onoff_indicator_off);
        icon.setLayoutParams(new LinearLayout.LayoutParams(60, 60));

        // Nút Sửa
        btnCancelEdit.setVisibility(View.VISIBLE);
        btnCancelEdit.setOnClickListener(v -> {
            editingScheduleId = -1;

            timePicker.setHour(6);
            timePicker.setMinute(30);
            editThreshold.setText("");
            switchAuto.setChecked(false);

            for (TextView tv : dayViews) {
                tv.setSelected(false);
                tv.setBackgroundColor(getResources().getColor(R.color.gray));
            }

            btnCancelEdit.setVisibility(View.GONE);
            Toast.makeText(this, "🆕 Đang ở chế độ thêm mới", Toast.LENGTH_SHORT).show();
        });
        Button btnEdit = new Button(this);
        btnEdit.setText("Sửa");
        btnEdit.setOnClickListener(v -> {
            editingScheduleId = id;

            timePicker.setHour(hour);
            timePicker.setMinute(minute);
            editThreshold.setText(String.valueOf(threshold)); // hoặc truyền thêm threshold
            switchAuto.setChecked(enabled);

            for (TextView tv : dayViews) {
                String day = tv.getText().toString().trim();
                tv.setSelected(repeatDays.contains(day));
                tv.setBackgroundColor(repeatDays.contains(day)
                        ? getResources().getColor(R.color.green)
                        : getResources().getColor(R.color.gray));
            }

            Toast.makeText(this, "📝 Đang chỉnh sửa lịch, nhấn 'Lưu' để cập nhật", Toast.LENGTH_SHORT).show();
        });

        // Nút Xóa
        Button btnDelete = new Button(this);
        btnDelete.setText("Xóa");
        // ✅ Đây là nơi bạn thêm xử lý khi bấm nút
        btnDelete.setOnClickListener(v -> {
            APIService api = RetrofitClientRaspi.getClient().create(APIService.class);
            api.deleteSchedule("delete", id).enqueue(new Callback<ResponseBody>() {
                @Override
                public void onResponse(Call<ResponseBody> call, Response<ResponseBody> response) {
                    Toast.makeText(PumpActivity.this, "🗑️ Đã xóa lịch", Toast.LENGTH_SHORT).show();
                    fetchSchedules(); // Tải lại danh sách
                }

                @Override
                public void onFailure(Call<ResponseBody> call, Throwable t) {
                    Toast.makeText(PumpActivity.this, "❌ Lỗi khi xóa", Toast.LENGTH_SHORT).show();
                }
            });
        });
        // Thêm các view vào dòng thời gian
        timeRow.addView(txtTime);
        timeRow.addView(icon);
        timeRow.addView(btnEdit);
        timeRow.addView(btnDelete);

        // Hàng hiển thị các ngày lặp lại
        LinearLayout daysRow = new LinearLayout(this);
        daysRow.setOrientation(LinearLayout.HORIZONTAL);
        daysRow.setGravity(Gravity.CENTER_VERTICAL);
        daysRow.setPadding(0, 12, 0, 0);

        for (String day : repeatDays.split(",")) {
            TextView dayView = new TextView(this);
            dayView.setText(day.trim());
            dayView.setTextColor(Color.parseColor("#4CAF50"));
            dayView.setTextSize(16);
            dayView.setPadding(12, 0, 12, 0);
            daysRow.addView(dayView);
        }

        item.addView(timeRow);
        item.addView(daysRow);
        layoutSchedules.addView(item);
    }


}
