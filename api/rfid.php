<?php
error_reporting(E_ALL);
ini_set('display_errors', 1);

$servername = "localhost";
$username = "root";
$password = "100803";
$dbname = "iot_data";

$conn = new mysqli($servername, $username, $password, $dbname);
if ($conn->connect_error) {
    die("Kết nối thất bại: " . $conn->connect_error);
}

// ✅ Cho phép lấy action từ cả GET và POST
$action = $_REQUEST['action'] ?? '';

// === 1. Đồng bộ dữ liệu từ server xuống ESP32/Android ===
if ($action === 'sync') {
    // Trả về dữ liệu JSON
    header('Content-Type: application/json');

    $pw_result = $conn->query("SELECT password FROM system_settings WHERE id = 1");
    $password = $pw_result ? $pw_result->fetch_assoc()['password'] : '12345';

    $rfid_result = $conn->query("SELECT * FROM rfid_table ORDER BY id ASC");
    $rfids = [];

    while ($rfid_result && $row = $rfid_result->fetch_assoc()) {
        $rfids[] = $row;
    }

    echo json_encode([
        "password" => $password,
        "rfid_list" => $rfids
    ]);

    $conn->close();
    exit;
}

// === 2. Thêm hoặc cập nhật thẻ RFID ===
elseif ($action === 'add') {
    $id = $_REQUEST['id'] ?? '';
    $uid1 = strtoupper($_REQUEST['uid1'] ?? '');
    $uid2 = strtoupper($_REQUEST['uid2'] ?? '');
    $uid3 = strtoupper($_REQUEST['uid3'] ?? '');
    $uid4 = strtoupper($_REQUEST['uid4'] ?? '');

    $sql = "INSERT INTO rfid_table (id, uid1, uid2, uid3, uid4) 
            VALUES ('$id', '$uid1', '$uid2', '$uid3', '$uid4') 
            ON DUPLICATE KEY UPDATE 
            uid1='$uid1', uid2='$uid2', uid3='$uid3', uid4='$uid4'";

    echo $conn->query($sql) === TRUE ? "✅ Saved" : "❌ Error: " . $conn->error;
    $conn->close();
    exit;
}

// === 3. Xóa thẻ RFID theo ID ===
elseif ($action === 'delete') {
    $id = $_REQUEST['id'] ?? '';
    $sql = "DELETE FROM rfid_table WHERE id = '$id'";
    echo $conn->query($sql) === TRUE ? "✅ Deleted" : "❌ Error: " . $conn->error;
    $conn->close();
    exit;
}

// === 4. Xóa tất cả thẻ RFID ===
elseif ($action === 'delete_all') {
    $sql = "DELETE FROM rfid_table";
    echo $conn->query($sql) === TRUE ? "✅ All Deleted" : "❌ Error: " . $conn->error;
    $conn->close();
    exit;
}

// === 5. Action không hợp lệ ===
else {
    echo "❌ Invalid action";
    $conn->close();
    exit;
}
?>
