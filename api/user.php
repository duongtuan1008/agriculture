<?php
header('Content-Type: application/json');
$conn = new mysqli("localhost", "root", "100803", "iot_data");

if ($conn->connect_error) {
    echo json_encode(["status" => "error", "message" => "Lỗi kết nối CSDL"]);
    exit;
}

// Đọc JSON gửi lên từ Android
$data = json_decode(file_get_contents("php://input"), true);
$action = $data["action"] ?? "";
$email = $data["email"] ?? "";
$password = $data["password"] ?? "";

// Kiểm tra thiếu thông tin
if (empty($email) || empty($password) || empty($action)) {
    echo json_encode(["status" => "error", "message" => "Thiếu dữ liệu"]);
    exit;
}

$email = $conn->real_escape_string($email);

// ==== 📥 ĐĂNG KÝ TÀI KHOẢN ====
if ($action === "register") {
    $password_hash = password_hash($password, PASSWORD_DEFAULT);

    $sql = "INSERT INTO users (email, password) VALUES (?, ?)";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("ss", $email, $password_hash);

    if ($stmt->execute()) {
        echo json_encode(["status" => "success", "message" => "Đăng ký thành công"]);
    } else {
        echo json_encode(["status" => "error", "message" => "Email đã tồn tại hoặc lỗi khác"]);
    }

    $stmt->close();
}

// ==== 🔐 ĐĂNG NHẬP TÀI KHOẢN ====
elseif ($action === "login") {
    $sql = "SELECT password FROM users WHERE email = ?";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("s", $email);
    $stmt->execute();
    $stmt->bind_result($hashed_password);

    if ($stmt->fetch()) {
        if (password_verify($password, $hashed_password)) {
            echo json_encode(["status" => "success", "message" => "Đăng nhập thành công"]);
        } else {
            echo json_encode(["status" => "error", "message" => "Sai mật khẩu"]);
        }
    } else {
        echo json_encode(["status" => "error", "message" => "Tài khoản không tồn tại"]);
    }

    $stmt->close();
}

// ==== 🚫 Không hợp lệ ====
else {
    echo json_encode(["status" => "error", "message" => "Action không hợp lệ"]);
}

$conn->close();
?>
