<?php
ini_set('display_errors', 1);
error_reporting(E_ALL);
header('Content-Type: application/json');

// --------------------------
// 1) KẾT NỐI CSDL
// --------------------------
$servername = "localhost";
$username   = "root";
$password   = "100803";
$dbname     = "iot_data";

$conn = new mysqli($servername, $username, $password, $dbname);
if ($conn->connect_error) {
    http_response_code(500);
    die(json_encode(["error" => "❌ Kết nối CSDL thất bại"]));
}

// Ở đây giả định device_id là 'esp32', có thể thay đổi tuỳ nhu cầu
$device_id = 'esp32';

// --------------------------
// 2) XOÁ LỊCH TƯỚI THEO ID
// --------------------------
if (isset($_GET['action']) && $_GET['action'] === 'delete' && isset($_GET['id'])) {
    $id = intval($_GET['id']);
    if ($id <= 0) {
        http_response_code(400);
        echo json_encode(["error" => "❌ ID không hợp lệ"]);
        exit;
    }

    $stmt = $conn->prepare("DELETE FROM control_config WHERE id = ?");
    $stmt->bind_param("i", $id);

    if ($stmt->execute()) {
        echo json_encode(["success" => "✅ Đã xóa lịch tưới (ID $id)"]);
    } else {
        http_response_code(500);
        echo json_encode(["error" => "❌ Lỗi khi xóa: " . $stmt->error]);
    }
    $stmt->close();
    $conn->close();
    exit;
}

// --------------------------
// 3) PHƯƠNG THỨC POST = THÊM / SỬA
// --------------------------
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $id              = isset($_POST['id']) ? intval($_POST['id']) : 0;
    $auto_mode       = isset($_POST['auto_mode']) ? intval($_POST['auto_mode']) : 0;
    $manual_override = isset($_POST['manual_override']) ? intval($_POST['manual_override']) : 0;
    $pump            = isset($_POST['pump']) ? intval($_POST['pump']) : 0;
    $start_hour      = isset($_POST['pump_start_hour']) ? intval($_POST['pump_start_hour']) : 6;
    $start_minute    = isset($_POST['pump_start_minute']) ? intval($_POST['pump_start_minute']) : 30;
    $flow_threshold  = isset($_POST['flow_threshold']) ? intval($_POST['flow_threshold']) : 500;
    $repeat_days_str = $_POST['repeat_days'] ?? "2";
    $is_enabled      = isset($_POST['is_enabled']) ? intval($_POST['is_enabled']) : 1;
    $notes           = $_POST['notes'] ?? null;

    if ($id > 0) {
        // 3A) CẬP NHẬT (KHÔNG tách repeat_days)
        $sql = "UPDATE control_config
                SET auto_mode=?, manual_override=?, pump=?,
                    pump_start_hour=?, pump_start_minute=?,
                    flow_threshold=?, repeat_days=?, is_enabled=?, notes=?, updated_at=NOW()
                WHERE id=?";
        $stmt = $conn->prepare($sql);
        $stmt->bind_param(
            "iiiiiisisi",
            $auto_mode,
            $manual_override,
            $pump,
            $start_hour,
            $start_minute,
            $flow_threshold,
            $repeat_days_str,
            $is_enabled,
            $notes,
            $id
        );

        if ($stmt->execute()) {
            echo json_encode(["success" => "✅ Đã cập nhật lịch tưới (ID $id), repeat_days = $repeat_days_str"]);
        } else {
            http_response_code(500);
            echo json_encode(["error" => "❌ Lỗi cập nhật: " . $stmt->error]);
        }
        $stmt->close();
        $conn->close();
        exit;
    } else {
        // 3B) THÊM MỚI (KHÔNG tách repeat_days)
        $sql = "INSERT INTO control_config
                (device_id, auto_mode, manual_override, pump,
                 pump_start_hour, pump_start_minute,
                 flow_threshold, repeat_days,
                 is_enabled, notes)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        $stmt = $conn->prepare($sql);
        $stmt->bind_param(
            "siiiiiisis",
            $device_id,
            $auto_mode,
            $manual_override,
            $pump,
            $start_hour,
            $start_minute,
            $flow_threshold,
            $repeat_days_str,
            $is_enabled,
            $notes
        );

        if ($stmt->execute()) {
            echo json_encode([
                "success" => "✅ Đã thêm 1 dòng với repeat_days = $repeat_days_str"
            ]);
        } else {
            http_response_code(500);
            echo json_encode(["error" => "❌ Lỗi thêm: " . $stmt->error]);
        }

        $stmt->close();
        $conn->close();
        exit;
    }
}

// --------------------------
// 4) GET = LẤY DANH SÁCH LỊCH TƯỚI
// --------------------------
if ($_SERVER['REQUEST_METHOD'] === 'GET' && isset($_GET['esp'])) {
    $sql = "SELECT * FROM control_config WHERE device_id = ? ORDER BY updated_at DESC";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("s", $device_id);
    $stmt->execute();
    $result = $stmt->get_result();

    $configs = [];
    while ($row = $result->fetch_assoc()) {
        $configs[] = $row;
    }
    $stmt->close();
    $conn->close();

    if (!empty($configs)) {
        echo json_encode($configs, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE);
    } else {
        http_response_code(404);
        echo json_encode(["error" => "❌ Không tìm thấy lịch tưới nào"]);
    }
    exit;
}

// --------------------------
// 5) PHƯƠNG THỨC KHÔNG HỢP LỆ
// --------------------------
http_response_code(400);
echo json_encode(["error" => "❌ Sai phương thức hoặc thiếu tham số"]);
$conn->close();
