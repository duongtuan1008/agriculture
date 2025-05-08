package com.example.myapplication;

import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.*;
import androidx.appcompat.app.AppCompatActivity;

import com.android.volley.*;
import com.android.volley.toolbox.*;

import org.json.*;

import java.util.*;
import android.content.Intent;
import android.view.Menu;
import android.view.MenuItem;
import androidx.annotation.NonNull;
import androidx.appcompat.widget.Toolbar;
import androidx.drawerlayout.widget.DrawerLayout;
import com.google.android.material.navigation.NavigationView;
import com.google.android.material.appbar.MaterialToolbar;
import androidx.core.view.GravityCompat;
import com.example.myapplication.RetrofitClientRaspi;

public class DoorActivity extends AppCompatActivity {

    EditText edtPassword, edtID, edtUID, edtDeleteID;
    Button btnUpdatePassword, btnAddRFID, btnDeleteRFID, btnSync;
    ListView listRFID;
    ArrayAdapter<String> adapter;
    ArrayList<String> rfidItems = new ArrayList<>();

    APIService api = RetrofitClientRaspi.getClient().create(APIService.class);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_door);

        // Gán ID các view
        edtPassword = findViewById(R.id.edtPassword);
        edtID = findViewById(R.id.edtID);
        edtUID = findViewById(R.id.edtUID);
        edtDeleteID = findViewById(R.id.edtDeleteID);

        btnUpdatePassword = findViewById(R.id.btnUpdatePassword);
        btnAddRFID = findViewById(R.id.btnAddRFID);
        btnDeleteRFID = findViewById(R.id.btnDeleteRFID);
        btnSync = findViewById(R.id.btnSync);

        DrawerLayout drawerLayout = findViewById(R.id.drawerLayoutDoor);
        NavigationView navView = findViewById(R.id.navigationViewDoor);
        MaterialToolbar toolbar = findViewById(R.id.toolbarDoor);
        setSupportActionBar(toolbar);

// Bấm icon trên toolbar để mở menu
        toolbar.setNavigationOnClickListener(v -> {
            drawerLayout.openDrawer(GravityCompat.END);
        });

// Xử lý chọn item trong menu
        navView.setNavigationItemSelectedListener(item -> {
            int id = item.getItemId();

            if (id == R.id.nav_home) {
                Toast.makeText(this, "Trang chủ", Toast.LENGTH_SHORT).show();
                startActivity(new Intent(DoorActivity.this, MainActivity.class));
            } else if (id == R.id.nav_pump) {
                startActivity(new Intent(DoorActivity.this, PumpActivity.class));

            } else if (id == R.id.nav_door) {
                Toast.makeText(this, "Bạn đang ở Cửa", Toast.LENGTH_SHORT).show();
            } else if (id == R.id.nav_logout) {
                getSharedPreferences("UserPrefs", MODE_PRIVATE)
                        .edit().putBoolean("isLoggedIn", false).apply();
                startActivity(new Intent(DoorActivity.this, LoginActivity.class));
                finish();
            }

            drawerLayout.closeDrawer(GravityCompat.END);
            return true;
        });



        listRFID = findViewById(R.id.listRFID);
        adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, rfidItems);
        listRFID.setAdapter(adapter);

        // Gọi API đồng bộ danh sách RFID và mật khẩu ngay khi mở app
        fetchRFIDList();
        getCurrentPassword();

        // Sự kiện nút
        btnUpdatePassword.setOnClickListener(v -> updatePassword());
        btnAddRFID.setOnClickListener(v -> addRFID());
        btnDeleteRFID.setOnClickListener(v -> deleteRFID());
        btnSync.setOnClickListener(v -> {
            fetchRFIDList();
            getCurrentPassword();
        });
    }

    private void updatePassword() {
        String newPass = edtPassword.getText().toString().trim();

        if (newPass.length() != 5) {
            Toast.makeText(this, "❌ Mật khẩu phải 5 ký tự", Toast.LENGTH_SHORT).show();
            return;
        }

        String url = RetrofitClientRaspi.getBaseUrl() + "password.php";


        StringRequest request = new StringRequest(Request.Method.POST, url,
                response -> Toast.makeText(this, "✅ " + response, Toast.LENGTH_LONG).show(),
                error -> Toast.makeText(this, "❌ Lỗi cập nhật: " + error.getMessage(), Toast.LENGTH_SHORT).show()) {

            @Override
            protected Map<String, String> getParams() {
                Map<String, String> map = new HashMap<>();
                map.put("action", "update_password");
                map.put("password", newPass);
                return map;
            }
        };

        Volley.newRequestQueue(this).add(request);
    }

    private void addRFID() {
        String id = edtID.getText().toString().trim();
        String uid = edtUID.getText().toString().trim();

        if (id.isEmpty() || uid.isEmpty()) {
            Toast.makeText(this, "❌ Nhập đầy đủ ID và UID", Toast.LENGTH_SHORT).show();
            return;
        }

        String[] parts = uid.split("-");
        if (parts.length != 4) {
            Toast.makeText(this, "❌ UID phải có 4 byte (VD: AB-CD-EF-01)", Toast.LENGTH_SHORT).show();
            return;
        }

        String url = RetrofitClientRaspi.getBaseUrl() + "rfid.php?action=add&id=" + id
                + "&uid1=" + parts[0] + "&uid2=" + parts[1]
                + "&uid3=" + parts[2] + "&uid4=" + parts[3];

        StringRequest request = new StringRequest(Request.Method.GET, url,
                response -> {
                    Toast.makeText(this, "✅ " + response, Toast.LENGTH_LONG).show();
                    fetchRFIDList(); // Tự đồng bộ sau khi thêm
                },
                error -> Toast.makeText(this, "❌ Lỗi thêm RFID", Toast.LENGTH_SHORT).show());

        Volley.newRequestQueue(this).add(request);
    }
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.top_menu, menu); // dùng chung menu
        return true;
    }

    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == R.id.menu_drawer) {
            DrawerLayout drawerLayout = findViewById(R.id.drawerLayoutDoor);
            drawerLayout.openDrawer(GravityCompat.END);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void deleteRFID() {
        String id = edtDeleteID.getText().toString().trim();

        if (id.isEmpty()) {
            Toast.makeText(this, "❌ Nhập ID cần xoá", Toast.LENGTH_SHORT).show();
            return;
        }

        String url = RetrofitClientRaspi.getBaseUrl()  + "rfid.php";

        StringRequest request = new StringRequest(Request.Method.POST, url,
                response -> {
                    Toast.makeText(this, "✅ " + response, Toast.LENGTH_LONG).show();
                    fetchRFIDList(); // Tự đồng bộ sau khi xoá
                },
                error -> Toast.makeText(this, "❌ Lỗi xoá RFID", Toast.LENGTH_SHORT).show()) {

            @Override
            protected Map<String, String> getParams() {
                Map<String, String> map = new HashMap<>();
                map.put("action", "delete");
                map.put("id", id);
                return map;
            }
        };

        Volley.newRequestQueue(this).add(request);
    }

    private void fetchRFIDList() {
        String url = RetrofitClientRaspi.getBaseUrl()  + "rfid.php?action=sync";

        JsonObjectRequest request = new JsonObjectRequest(Request.Method.GET, url, null,
                response -> {
                    try {
                        Log.d("RFID_RESPONSE", response.toString());
                        JSONArray rfidList = response.getJSONArray("rfid_list");
                        rfidItems.clear();
                        for (int i = 0; i < rfidList.length(); i++) {
                            JSONObject obj = rfidList.getJSONObject(i);
                            String item = "ID: " + obj.getString("id") +
                                    " → UID: " + obj.getString("uid1") + " "
                                    + obj.getString("uid2") + " "
                                    + obj.getString("uid3") + " "
                                    + obj.getString("uid4");
                            rfidItems.add(item);
                        }
                        adapter.notifyDataSetChanged();
                    } catch (JSONException e) {
                        e.printStackTrace();
                        Toast.makeText(this, "❌ JSON lỗi: " + e.getMessage(), Toast.LENGTH_SHORT).show();
                    }
                },
                error -> {
                    error.printStackTrace();
                    Toast.makeText(this, "❌ Lỗi đồng bộ: " + error.getMessage(), Toast.LENGTH_SHORT).show();
                });

        Volley.newRequestQueue(this).add(request);
    }

    private void getCurrentPassword() {
        String url = RetrofitClientRaspi.getBaseUrl() + "password.php?action=get_password";

        JsonObjectRequest request = new JsonObjectRequest(Request.Method.GET, url, null,
                response -> {
                    try {
                        String currentPass = response.getString("password");
                        edtPassword.setText(currentPass); // Hiển thị vào EditText
                    } catch (JSONException e) {
                        e.printStackTrace();
                        Toast.makeText(this, "❌ JSON lỗi khi lấy mật khẩu", Toast.LENGTH_SHORT).show();
                    }
                },
                error -> {
                    error.printStackTrace();
                    Toast.makeText(this, "❌ Lỗi khi lấy mật khẩu: " + error.getMessage(), Toast.LENGTH_SHORT).show();
                });

        Volley.newRequestQueue(this).add(request);
    }
}
