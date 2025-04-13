<?php
// Hiển thị lỗi
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

// Header JSON & Chống cache
header('Content-Type: application/json');
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
header("Pragma: no-cache");
header("Expires: 0");

// Kết nối CSDL
$mysqli = new mysqli("localhost", "root", "100803", "iot_data");
if ($mysqli->connect_error) {
    http_response_code(500);
    echo json_encode(["status" => "error", "message" => "❌ Lỗi kết nối CSDL"]);
    exit;
}

// =====================
// 📤 POST: Cập nhật trạng thái thiết bị
// =====================
if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $device = $_POST['device'] ?? '';
    $state  = $_POST['state'] ?? '';
    $source = $_POST['source'] ?? 'app';

    if (empty($device) || empty($state)) {
        http_response_code(400);
        echo json_encode(["status" => "error", "message" => "❗ Thiếu device hoặc state"]);
        exit;
    }

    // Ghi log
    $stmt1 = $mysqli->prepare("INSERT INTO device_logs (device, action, source) VALUES (?, ?, ?)");
    if (!$stmt1) {
        http_response_code(500);
        echo json_encode(["status" => "error", "message" => "❌ Lỗi prepare log: " . $mysqli->error]);
        exit;
    }
    $stmt1->bind_param("sss", $device, $state, $source);
    $stmt1->execute();
    $stmt1->close();

    // Cập nhật trạng thái hiện tại
    $stmt2 = $mysqli->prepare("REPLACE INTO device_state (device, state) VALUES (?, ?)");
    if (!$stmt2) {
        http_response_code(500);
        echo json_encode(["status" => "error", "message" => "❌ Lỗi prepare state: " . $mysqli->error]);
        exit;
    }
    $stmt2->bind_param("ss", $device, $state);
    $stmt2->execute();
    $stmt2->close();

    echo json_encode(["status" => "ok", "message" => "✅ $device đã cập nhật thành $state"]);
    exit;
}

// =====================
// 📥 GET: Lấy trạng thái thiết bị
// =====================
elseif ($_SERVER["REQUEST_METHOD"] === "GET") {
    $result = $mysqli->query("SELECT * FROM device_state");
    $states = [];

    while ($row = $result->fetch_assoc()) {
        $states[$row['device']] = $row['state'];
    }

    // Đảm bảo có mặc định
    $states = array_merge([
        "pump" => "OFF",
        "led"  => "OFF"
    ], $states);

    echo json_encode($states);
    exit;
}

// =====================
// ❌ Nếu không phải GET hoặc POST
// =====================
http_response_code(405);
echo json_encode(["status" => "error", "message" => "❌ Chỉ hỗ trợ POST và GET"]);
exit;

$mysqli->close();
