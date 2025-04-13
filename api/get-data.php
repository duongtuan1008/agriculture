<?php
// Hiển thị lỗi khi debug
ini_set('display_errors', 1);
error_reporting(E_ALL);
header('Content-Type: application/json');

// Cấu hình MySQL
$servername = "localhost";
$username = "root";
$password = "100803";
$dbname = "iot_data";
$api_key_value = "tPmAT5Ab3j7F9";

// Kết nối MySQL
$conn = new mysqli($servername, $username, $password, $dbname);
if ($conn->connect_error) {
    echo json_encode([]);
    exit;
}

// 📥 XỬ LÝ GET – trả về JSON
if ($_SERVER['REQUEST_METHOD'] === 'GET') {
    $sql = "SELECT * FROM sensor_data ORDER BY id DESC LIMIT 1";
    $result = $conn->query($sql);
    $data = [];

    if ($result && $row = $result->fetch_assoc()) {
        $data[] = $row;
    }

    echo json_encode($data);
    $conn->close();
    exit;
}

// 📤 XỬ LÝ POST từ ESP
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $api_key     = $_POST['api_key'] ?? '';
    $device_id   = $_POST['device_id'] ?? 'esp32';
    $temperature = $_POST['temperature'] ?? 0;
    $humidity    = $_POST['humidity'] ?? 0;
    $flame       = $_POST['flame'] ?? 0;
    $light       = $_POST['light'] ?? 0;
    $flow        = $_POST['flow'] ?? 0;
    $soil        = $_POST['soil'] ?? 0;
    $rain        = $_POST['rain'] ?? 0;

    if ($api_key === $api_key_value) {
        $stmt = $conn->prepare("INSERT INTO sensor_data 
            (temperature, humidity, flame, light, flow, soil, rain, device_id)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        $stmt->bind_param("diddiids", $temperature, $humidity, $flame, $light, $flow, $soil, $rain, $device_id);
        $stmt->execute();
        $stmt->close();

        echo json_encode(["status" => "ok", "message" => "Dữ liệu đã lưu"]);
    } else {
        echo json_encode(["status" => "error", "message" => "Sai API Key"]);
    }

    $conn->close();
    exit;
}

// ❌ Nếu không phải GET hoặc POST
echo json_encode([]);
$conn->close();
exit;
?>
