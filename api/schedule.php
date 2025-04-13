<?php
// Hiển thị lỗi
ini_set('display_errors', 1);
error_reporting(E_ALL);

// Header JSON + chống cache
header('Content-Type: application/json');
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");

// Kết nối CSDL
$mysqli = new mysqli("localhost", "root", "100803", "iot_data");
if ($mysqli->connect_error) {
    http_response_code(500);
    echo json_encode(["status" => "error", "message" => "Lỗi CSDL"]);
    exit;
}

// Lấy danh sách lịch
$query = "SELECT id, device, hour, minute, duration FROM schedule ORDER BY hour, minute";
$result = $mysqli->query($query);

$schedules = [];

while ($row = $result->fetch_assoc()) {
    $schedules[] = [
        "id"       => (int)$row['id'],
        "device"   => $row['device'],
        "hour"     => (int)$row['hour'],
        "minute"   => (int)$row['minute'],
        "duration" => (int)$row['duration']
    ];
}

// Trả về JSON
echo json_encode($schedules);
$mysqli->close();
?>

