═══════════════════════════════════════════════════════════════════
 PRISM — HƯỚNG DẪN DÀNH CHO TESTER
═══════════════════════════════════════════════════════════════════

YÊU CẦU MÁY TÍNH
────────────────
 • Windows 10 (64-bit) trở lên — KHÔNG cần cài thêm gì cả.
   (không cần Qt, không cần .NET, không cần Visual C++ Redistributable)
 • Ổ đĩa trống: ~160 MB.

CÁCH CÀI
────────
 Cách 1 — INSTALLER (khuyến nghị):
   1. Chạy file  Prism-Setup.exe  (double-click).
   2. Nhấn Install, chờ vài giây — xong!
   3. Shortcut "Prism" sẽ có trên Desktop và trong Start Menu.
   4. Gỡ bỏ: Settings ▸ Apps ▸ Prism ▸ Uninstall
      (hoặc Start Menu ▸ Prism ▸ Uninstall Prism).

 Cách 2 — PORTABLE (không cần cài):
   1. Giải nén Prism-win64-*.zip ra thư mục bất kỳ.
   2. Chạy thẳng prism.exe trong đó.

 Nếu Windows hỏi "Windows protected your PC" khi chạy installer:
 nhấn More info → Run anyway (installer không tải gì từ mạng).

BÁO LỖI NHƯ THẾ NÀO
────────────────────
 Khi gặp lỗi/crash, gửi kèm file log:
   %APPDATA%\Prism\Prism\prism.log
 (dán đường dẫn trên vào Explorer là tới). File log ghi lại lỗi QML
 và kết quả các thao tác — có nó là tìm nguyên nhân nhanh hơn nhiều.)

DANH SÁCH NÊN TEST
──────────────────
 1. Tạo document mới (New) / mở ảnh PNG, JPG / lưu .prism / Export PNG, JPG.
 2. Vẽ bằng brush: đổi Size, Opacity, Hardness bằng cả slider và ô số.
 3. Eraser: vẽ rồi tẩy — kiểm tra undo (Ctrl+Z) / redo (Ctrl+Shift+Z).
 4. Layers:
    • Thêm layer (+), đổi tên (double-click vào tên), ẩn/hiện, lock.
    • KÉO-THẢ dòng layer để đổi thứ tự; nút mũi tên lên/xuống (Raise/Lower).
    • Kéo opacity slider — số % trên dòng layer phải đổi theo NGAY khi kéo.
    • Blend mode: thử Multiply, Screen, Color Burn, Luminosity...
    • Layer ▸ Import Image as Layer... (chọn ảnh JPG/PNG — kiểm tra
      ảnh lớn hơn canvas sẽ bị cắt và có thông báo ở thanh trạng thái).
 5. Selection: Select All, Deselect, wand, lasso; vẽ trong vùng chọn.
 6. Transform tool, crop, fill, text, các shape (line/rect/ellipse).
 7. Filters: Invert, Desaturate, Blur, Brightness/Contrast.
 8. Đổi hotkey: Settings ▸ Configure Tool Shortcuts (gõ phím trùng sẽ
    có cảnh báo vàng).
 9. Zoom slider ở góc phải dưới + phím tắt +/-.
10. Lưu project rồi mở lại — kiểm tra layers/blend/opacity còn đúng.

GHI CHÚ BIẾT LỖI (đã biết, chưa sửa)
─────────────────────────────────────
 • Chưa có layer group (thư mục) / clipping mask / alpha lock.
 • Image ▸ Resize Canvas... chưa hoạt động (đang disable).
 • Crop không undo được.
 • Transform xoay bằng kéo ngang, không phải khung 8 handle như Krita.

CẢM ƠN BẠN ĐÃ TEST!
