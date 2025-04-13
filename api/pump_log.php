<?php
// Hiện tất cả lỗi khi debug
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

// Thông tin kết nối database
$servername = "localhost";
$username = "root";
$password = "100803";
$dbname = "iot_data";
$api_key_value = "tPmAT5Ab3j7F9";

// Kết nối MySQL
$conn = new mysqli($servername, $username, $password, $dbname);
if ($conn->connect_error) {
    die("❌ Kết nối thất bại: " . $conn->connect_error);
}

// Xử lý khi có POST
if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $api_key   = $_POST['api_key'] ?? '';
    $device_id = $_POST['device_id'] ?? 'esp32';
    $volume    = floatval($_POST['volume'] ?? 0);
    $status    = $_POST['status'] ?? '';
    $time      = $_POST['time'] ?? date("Y-m-d H:i:s");

    // ✅ Kiểm tra khóa và trạng thái hợp lệ
    if ($api_key === $api_key_value && $status === "done") {

        // Kiểm tra định dạng thời gian (nên là yyyy-mm-dd HH:MM:SS)
        $datetime = date('Y-m-d H:i:s', strtotime($time));

        $stmt = $conn->prepare("INSERT INTO pump_logs (device_id, volume_ml, log_time, status) VALUES (?, ?, ?, ?)");
        if (!$stmt) {
            die("❌ Lỗi prepare: " . $conn->error);
        }

        $stmt->bind_param("sdss", $device_id, $volume, $datetime, $status);

        if ($stmt->execute()) {
            echo "✅ Đã lưu log hoàn thành bơm";
        } else {
            echo "❌ Lỗi khi lưu log: " . $stmt->error;
        }

        $stmt->close();
    } else {
        echo "❌ Sai API Key hoặc trạng thái không hợp lệ!";
    }
}

$conn->close();
?>
