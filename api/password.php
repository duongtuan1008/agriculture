<?php
error_reporting(E_ALL);
ini_set('display_errors', 1);

// Kết nối MySQL
$servername = "localhost";
$username = "root";
$password = "100803";
$dbname = "iot_data";

$conn = new mysqli($servername, $username, $password, $dbname);
if ($conn->connect_error) {
    die(json_encode(["error" => "❌ Kết nối thất bại: " . $conn->connect_error]));
}

// Cho phép lấy action từ cả GET và POST
$action = $_REQUEST['action'] ?? '';
$default_pass = "12395"; // Mật khẩu mặc định

header('Content-Type: application/json'); // Tất cả phản hồi đều ở dạng JSON

// ✅ 1. Cập nhật mật khẩu mới
if ($action === 'update_password') {
    $new_pass = $_REQUEST['password'] ?? '';

    if (strlen($new_pass) !== 5) {
        echo json_encode(["error" => "❌ Password phải đúng 5 ký tự"]);
    } else {
        $sql = "UPDATE system_settings SET password = '$new_pass' WHERE id = 1";
        if ($conn->query($sql) === TRUE) {
            echo json_encode(["message" => "✅ Password Updated"]);
        } else {
            echo json_encode(["error" => "❌ Lỗi: " . $conn->error]);
        }
    }
}

// ✅ 2. Đặt lại mật khẩu mặc định
elseif ($action === 'reset_password') {
    $sql = "UPDATE system_settings SET password = '$default_pass' WHERE id = 1";
    if ($conn->query($sql) === TRUE) {
        echo json_encode(["message" => "✅ Password Reset to Default"]);
    } else {
        echo json_encode(["error" => "❌ Lỗi: " . $conn->error]);
    }
}

// ✅ 3. Lấy mật khẩu hiện tại
elseif ($action === 'get_password') {
    $result = $conn->query("SELECT password FROM system_settings WHERE id = 1");
    if ($result && $row = $result->fetch_assoc()) {
        echo json_encode(["password" => $row['password']]);
    } else {
        echo json_encode(["error" => "❌ Không tìm thấy mật khẩu"]);
    }
}

// ❌ Action không hợp lệ
else {
    echo json_encode(["error" => "❌ Invalid action"]);
}

$conn->close();
?>
