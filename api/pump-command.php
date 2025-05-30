<?php
// Hiển thị lỗi khi debug
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

// Trả về JSON + chống cache
header('Content-Type: application/json');
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
header("Pragma: no-cache");
header("Expires: 0");

// Kết nối CSDL
$mysqli = new mysqli("localhost", "root", "100803", "iot_data");
if ($mysqli->connect_error) {
    http_response_code(500);
    echo json_encode([
        "status" => "error",
        "message" => "Lỗi kết nối cơ sở dữ liệu"
    ]);
    exit;
}

// =====================
// 📤 POST: Cập nhật trạng thái thiết bị
// =====================
if ($_SERVER["REQUEST_METHOD"] === "POST") {
    // Lấy dữ liệu JSON từ body yêu cầu
    $data = json_decode(file_get_contents("php://input"), true);

    // Kiểm tra nếu dữ liệu JSON hợp lệ
    if ($data === null) {
        http_response_code(400);
        echo json_encode([ // Trả về lỗi nếu JSON không hợp lệ
            "status" => "error",
            "message" => "Dữ liệu JSON không hợp lệ"
        ]);
        exit;
    }

    $device = $data['device'] ?? '';  // Lấy thông tin thiết bị từ JSON
    $state  = $data['state'] ?? '';   // Lấy trạng thái của thiết bị từ JSON

    // Kiểm tra thiếu thiết bị hoặc trạng thái
    if (empty($device) || empty($state)) {
        http_response_code(400);
        echo json_encode([ // Trả về lỗi nếu thiếu device hoặc state
            "status" => "error",
            "message" => "Thiếu device hoặc state"
        ]);
        exit;
    }

    // Kiểm tra thiết bị hợp lệ
    $allowed_devices = ["pump", "led", "curtain"]; // 🆕 Thêm "curtain" vào danh sách thiết bị hợp lệ
    if (!in_array($device, $allowed_devices)) {
        http_response_code(400);
        echo json_encode([ // Trả về lỗi nếu thiết bị không hợp lệ
            "status" => "error",
            "message" => "Thiết bị không hợp lệ"
        ]);
        exit;
    }

    // Cập nhật trạng thái thiết bị vào cơ sở dữ liệu
    $stmt = $mysqli->prepare("REPLACE INTO device_state (device, state) VALUES (?, ?)");
    if (!$stmt) {
        http_response_code(500);
        echo json_encode([ // Trả về lỗi nếu lỗi khi chuẩn bị câu lệnh SQL
            "status" => "error",
            "message" => "Lỗi prepare: " . $mysqli->error
        ]);
        exit;
    }

    $stmt->bind_param("ss", $device, $state);
    $stmt->execute();
    $stmt->close();

    echo json_encode([ // Trả về thông báo thành công khi cập nhật thành công
        "status" => "ok",
        "message" => "$device đã được cập nhật thành $state"
    ]);
    exit;
}

// =====================
// 📥 GET: Lấy trạng thái thiết bị (cho ESP hoặc App)
// =====================
elseif ($_SERVER["REQUEST_METHOD"] === "GET") {
    $result = $mysqli->query("SELECT * FROM device_state");
    $states = [];

    // Lấy trạng thái của tất cả các thiết bị
    while ($row = $result->fetch_assoc()) {
        $states[$row['device']] = $row['state'];
    }

    // Đảm bảo có các thiết bị mặc định: pump, led, curtain
    $states = array_merge([
        "pump"    => "OFF",   // Mặc định pump tắt
        "led"     => "OFF",   // Mặc định led tắt
        "curtain" => "OFF"    // 🆕 Mặc định màn che tắt
    ], $states);

    echo json_encode($states); // Trả về trạng thái của tất cả các thiết bị
    exit; // ✅ BẮT BUỘC
}

// =====================
// ❌ Nếu không phải GET hoặc POST
// =====================
http_response_code(405);
echo json_encode([ // Trả về lỗi nếu không phải phương thức GET hoặc POST
    "status" => "error",
    "message" => "Chỉ hỗ trợ POST và GET"
]);
exit;

$mysqli->close();
?>
