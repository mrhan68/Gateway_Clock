import docx
from docx import Document
from docx.shared import Inches, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_ALIGN_VERTICAL
from docx.oxml import OxmlElement, parse_xml
from docx.oxml.ns import nsdecls, qn

def set_cell_background(cell, fill_hex):
    tcPr = cell._tc.get_or_add_tcPr()
    shd = parse_xml(f'<w:shd {nsdecls("w")} w:fill="{fill_hex}"/>')
    tcPr.append(shd)

def set_cell_margins(cell, top=100, bottom=100, left=150, right=150):
    tcPr = cell._tc.get_or_add_tcPr()
    tcMar = parse_xml(f'''<w:tcMar {nsdecls("w")}>
        <w:top w:w="{top}" w:type="dxa"/>
        <w:bottom w:w="{bottom}" w:type="dxa"/>
        <w:left w:w="{left}" w:type="dxa"/>
        <w:right w:w="{right}" w:type="dxa"/>
    </w:tcMar>''')
    tcPr.append(tcMar)

def set_table_borders(table, color="CCCCCC", sz="4", val="single"):
    tblPr = table._tbl.tblPr
    borders = parse_xml(f'''<w:tblBorders {nsdecls("w")}>
        <w:top w:val="{val}" w:sz="{sz}" w:space="0" w:color="{color}"/>
        <w:bottom w:val="{val}" w:sz="{sz}" w:space="0" w:color="{color}"/>
        <w:insideH w:val="{val}" w:sz="{sz}" w:space="0" w:color="{color}"/>
        <w:insideV w:val="{val}" w:sz="{sz}" w:space="0" w:color="{color}"/>
        <w:left w:val="{val}" w:sz="{sz}" w:space="0" w:color="{color}"/>
        <w:right w:val="{val}" w:sz="{sz}" w:space="0" w:color="{color}"/>
    </w:tblBorders>''')
    tblPr.append(borders)

doc = Document()

# Page Margins: Left 3cm (~1.18 in), Right 2cm (~0.79 in), Top 2cm, Bottom 2cm
for section in doc.sections:
    section.top_margin = Inches(0.8)
    section.bottom_margin = Inches(0.8)
    section.left_margin = Inches(1.1)
    section.right_margin = Inches(0.8)

# Base Style Settings: Times New Roman
style = doc.styles['Normal']
font = style.font
font.name = 'Times New Roman'
font.size = Pt(12)
font.color.rgb = RGBColor(0x22, 0x22, 0x22)
style.paragraph_format.line_spacing = 1.2
style.paragraph_format.space_after = Pt(4)

# Header Section
header_tbl = doc.add_table(rows=1, cols=2)
header_tbl.alignment = WD_TABLE_ALIGNMENT.CENTER
header_tbl.autofit = False

cell_l, cell_r = header_tbl.rows[0].cells
cell_l.width = Inches(3.2)
cell_r.width = Inches(3.2)

p_l = cell_l.paragraphs[0]
p_l.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p_l.add_run("ĐẠI HỌC BÁCH KHOA HÀ NỘI\n")
r.font.bold = True
r.font.size = Pt(11)
r = p_l.add_run("TRƯỜNG CÔNG NGHỆ THÔNG TIN & TRUYỀN THÔNG\n")
r.font.bold = True
r.font.size = Pt(10.5)
r.font.color.rgb = RGBColor(0x8B, 0x00, 0x00) # HUST red
r = p_l.add_run("-----------------------")
r.font.size = Pt(9)

p_r = cell_r.paragraphs[0]
p_r.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p_r.add_run("CỘNG HÒA XÃ HỘI CHỦ NGHĨA VIỆT NAM\n")
r.font.bold = True
r.font.size = Pt(11)
r = p_r.add_run("Độc lập – Tự do – Hạnh phúc\n")
r.font.bold = True
r.font.size = Pt(11)
r = p_r.add_run("-----------------------")
r.font.size = Pt(9)

doc.add_paragraph().paragraph_format.space_after = Pt(6)

# Title
title_p = doc.add_paragraph()
title_p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r_title = title_p.add_run("BÁO CÁO TIẾN ĐỘ THỰC HIỆN ĐỒ ÁN TỐT NGHIỆP\n")
r_title.font.bold = True
r_title.font.size = Pt(16)
r_title.font.color.rgb = RGBColor(0x00, 0x33, 0x66)

r_sub = title_p.add_run("HỆ CỬ NHÂN KỸ THUẬT / CÔNG NGHỆ THÔNG TIN — HỌC KỲ 2026-1")
r_sub.font.bold = True
r_sub.font.size = Pt(11)
r_sub.font.color.rgb = RGBColor(0x55, 0x55, 0x55)

doc.add_paragraph().paragraph_format.space_after = Pt(4)

# Project & Student Info Box
info_tbl = doc.add_table(rows=6, cols=2)
info_tbl.alignment = WD_TABLE_ALIGNMENT.CENTER
set_table_borders(info_tbl, color="B0C4DE", sz="6")

info_data = [
    ("Tên đề tài:", "Xây dựng hệ thống đồng hồ thông minh tích hợp Edge IoT Gateway"),
    ("Lĩnh vực đề tài:", "Internet of Things (IoT), Hệ thống nhúng (Embedded Systems), Kỹ thuật phần mềm"),
    ("Sinh viên thực hiện:", "[Họ và tên sinh viên]           MSSV: [Mã số SV]           Lớp: [Tên lớp]"),
    ("Giáo viên hướng dẫn:", "[Họ và tên Thầy / Cô GVHD]"),
    ("Thời gian thực hiện:", "17 tuần (Từ tháng 09/2026 đến tháng 01/2027)"),
    ("Thời điểm báo cáo:", "Tuần 15 – 16 / 17 tuần (Đã hoàn tất thử nghiệm phần cứng thật)")
]

for i, (k, v) in enumerate(info_data):
    row = info_tbl.rows[i]
    cell_k, cell_v = row.cells
    cell_k.width = Inches(2.0)
    cell_v.width = Inches(4.5)
    
    set_cell_background(cell_k, "F4F8FA")
    set_cell_margins(cell_k, 80, 80, 120, 120)
    set_cell_margins(cell_v, 80, 80, 120, 120)
    
    p_k = cell_k.paragraphs[0]
    p_k.paragraph_format.space_after = Pt(0)
    r_k = p_k.add_run(k)
    r_k.font.bold = True
    r_k.font.size = Pt(11)
    
    p_v = cell_v.paragraphs[0]
    p_v.paragraph_format.space_after = Pt(0)
    r_v = p_v.add_run(v)
    r_v.font.size = Pt(11)
    if k == "Tên đề tài:":
        r_v.font.bold = True
        r_v.font.color.rgb = RGBColor(0x00, 0x33, 0x66)

doc.add_paragraph().paragraph_format.space_after = Pt(8)

def add_heading_1(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(12)
    p.paragraph_format.space_after = Pt(4)
    r = p.add_run(text)
    r.font.bold = True
    r.font.size = Pt(13)
    r.font.color.rgb = RGBColor(0x00, 0x33, 0x66)
    return p

def add_heading_2(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(8)
    p.paragraph_format.space_after = Pt(2)
    r = p.add_run(text)
    r.font.bold = True
    r.font.size = Pt(11.5)
    r.font.color.rgb = RGBColor(0x11, 0x55, 0x88)
    return p

# Section I
add_heading_1("I. TỔNG QUAN TIẾN ĐỘ ĐỐI SOÁT THEO PHIẾU GIAO NHIỆM VỤ")

p_intro = doc.add_paragraph()
p_intro.add_run(
    "Căn cứ Phiếu Giao Nhiệm Vụ (PGNV) Đồ án tốt nghiệp hệ cử nhân của Trường CNTT&TT – ĐHBK Hà Nội, "
    "đồ án được phân bổ thực hiện trong 17 tuần làm việc (12 tiết/tuần). Dưới đây là bảng tổng hợp tiến độ đối soát thực tế:"
)

tbl_pgnv = doc.add_table(rows=6, cols=5)
tbl_pgnv.alignment = WD_TABLE_ALIGNMENT.CENTER
set_table_borders(tbl_pgnv, color="CCCCCC")

pgnv_headers = ["STT", "Nội dung giao việc theo PGNV", "Kế hoạch", "Tiến độ", "Đánh giá nghiệm thu"]
hdr_row = tbl_pgnv.rows[0]
for idx, title in enumerate(pgnv_headers):
    cell = hdr_row.cells[idx]
    set_cell_background(cell, "003366")
    set_cell_margins(cell, 100, 100, 100, 100)
    p = cell.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(0)
    r = p.add_run(title)
    r.font.bold = True
    r.font.size = Pt(10)
    r.font.color.rgb = RGBColor(0xFF, 0xFF, 0xFF)

pgnv_rows = [
    ("1", "Khảo sát bài toán & thiết kế hệ thống:\n• Khảo sát nhược điểm Cloud-only, chuẩn BLE 5.0 (0x181A, BTHome V2)\n• Lập đặc tả kỹ thuật FSD và thiết kế kiến trúc phân tầng 3 trụ cột", "Tuần 1–3", "100%", "HOÀN TẤT\n(FSD v2.1 đóng băng)"),
    ("2", "Nghiên cứu phần cứng & firmware (ESP32-S3):\n• Đấu nối ngoại vi (OLED, RTC DS3231, thẻ MicroSD, nút bấm, buzzer)\n• Lập trình FreeRTOS đa lõi, Event Bus, RTC/SNTP, BLE NimBLE thụ động\n• Ghi nhật ký FATFS thẻ nhớ, Task Watchdog TWDT giám sát nhịp tim", "Tuần 4–8", "100%", "HOÀN TẤT\n(Đo kiểm trên bo mạch thật)"),
    ("3", "Xây dựng máy chủ Cloud Backend & CSDL:\n• REST API Node.js/Express, MQTTS TLS 8883 (EMQX Cloud)\n• CSDL quan hệ PostgreSQL / SQLite (Prisma ORM)\n• Xác thực JWT, scrypt, thuật toán khử trùng gửi bù (Backfill Dedup)", "Tuần 9–11", "100%", "HOÀN TẤT\n(20/20 Jest tests PASS)"),
    ("4", "Xây dựng Web Dashboard & Tích hợp:\n• React 18, Vite, TailwindCSS (chuẩn Impeccable, 0 anti-patterns)\n• Giám sát thời gian thực, quản lý cảm biến, đặt lịch báo thức\n• Tích hợp liên thông 3 tầng, kiểm thử gửi bù Store-and-Forward", "Tuần 12–14", "100%", "HOÀN TẤT\n(Build Vite sạch 100%)"),
    ("5", "Thử nghiệm thực tế, đánh giá hiệu năng & báo cáo:\n• Đo kiểm ổn định liên tục, thời gian boot màn hình, rò rỉ bộ nhớ RAM\n• Thử nghiệm kịch bản lỗi: mất Wi-Fi, rút nóng thẻ nhớ, ngắt nguồn 5V\n• Đánh giá chỉ số hiệu năng và hoàn thiện quyển Báo cáo Đồ án", "Tuần 15–17", "90%", "ĐANG VIẾT BÁO CÁO\n(Đã xong 100% đo kiểm thật)")
]

col_widths = [Inches(0.4), Inches(2.8), Inches(0.9), Inches(0.8), Inches(1.6)]

for row_idx, rdata in enumerate(pgnv_rows):
    row = tbl_pgnv.rows[row_idx + 1]
    bg_color = "F9FBFC" if row_idx % 2 == 1 else "FFFFFF"
    for col_idx, text in enumerate(rdata):
        cell = row.cells[col_idx]
        cell.width = col_widths[col_idx]
        set_cell_background(cell, bg_color)
        set_cell_margins(cell, 80, 80, 100, 100)
        p = cell.paragraphs[0]
        p.paragraph_format.space_after = Pt(0)
        if col_idx in (0, 2, 3):
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = p.add_run(text)
        r.font.size = Pt(9.5)
        if col_idx == 3:
            r.font.bold = True
            r.font.color.rgb = RGBColor(0x00, 0x88, 0x00)
        elif col_idx == 4 and "HOÀN TẤT" in text:
            r.font.bold = True

doc.add_paragraph().paragraph_format.space_after = Pt(8)

# Section II
add_heading_1("II. CHI TIẾT KẾT QUẢ ĐÃ ĐẠT ĐƯỢC THEO TỪNG PHÂN HỆ")

add_heading_2("1. Phân hệ Thiết bị biên (ESP32-S3 Edge Gateway Firmware)")
p = doc.add_paragraph()
p.add_run(
    "Thiết bị phần cứng đã được lắp ráp mạch hoàn chỉnh và nạp firmware phiên bản mới nhất, kết nối trực tiếp "
    "qua cổng USB UART0 (/dev/ttyACM0, 115200 baud). Toàn bộ các tính năng đã được đo kiểm thực chứng:"
)

firmware_points = [
    ("Bộ xử lý trung tâm đa lõi:", " ESP32-S3 (Dual-core Xtensa LX7 @ 160MHz, 16MB SPI Flash, 8MB PSRAM). Core 0 đảm nhiệm tác vụ mạng Wi-Fi, MQTTS TLS, Web Server và Task Supervisor; Core 1 xử lý màn hình OLED, RTC, quét BLE và ghi thẻ nhớ MicroSD."),
    ("Màn hình OLED SSD1327 (1.5\" 128x128 I2C0 0x3C):", " Khởi động hiển thị siêu tốc (< 0.25 giây ngay tại boot segment 7). Giao diện phân trang trực quan: Màn hình 0 (Đồng hồ số lớn + Nhiệt ẩm môi trường + Trạng thái mạng), Màn hình 1 (Danh sách cảm biến BLE), Màn hình 2 (Trạng thái hệ thống). Tự động hiển thị trang Provisioning khi chưa kết nối Wi-Fi."),
    ("Mạch thời gian thực RTC DS3231 (I2C0 0x68) + Pin CR2032:", " Tích hợp thạch anh bù nhiệt TCXO độ chính xác cao. Đồng bộ 1 chiều từ SNTP (pool.ntp.org) nạp vào thanh ghi phần cứng khi có Wi-Fi. Đã kiểm chứng rút nguồn USB 5V hoàn toàn: pin CR2032 duy trì giờ tuyệt đối chính xác, khi cắm lại không bị nhảy về năm 1970."),
    ("Thẻ nhớ MicroSD FAT32 (SPI2 CS=10, MOSI=11, CLK=12, MISO=13):", " Tự động tạo thư mục /DATA và /LOGS. Ghi nhật ký CSV đo đạc môi trường theo ngày và lưu trữ hàng đợi nhị phân 172-byte sync_queue.bin theo triết lý Offline-First. Đã kiểm chứng rút nóng thẻ nhớ: vòng đệm RAM 50 items kích hoạt an toàn; khi cắm lại, hệ thống drain toàn bộ vào thẻ và gửi bù (backfill) lên Cloud MQTT broker."),
    ("Nạp cấu hình thẻ nhớ (TC-CFG-01):", " Lệnh serial 'config import /sdcard/config.json' đọc cấu hình JSON, nạp an toàn vào NVS Flash, tự đổi tên thành .bak và reload cấu hình động không cần reset."),
    ("Nút bấm & Báo thức:", " Nút BOOT (GPIO 0) tích hợp thông minh: nhả nhanh (<500ms) chuyển trang màn hình; nhấn giữ (>=2s) tắt chuông báo thức; nhấn giữ (>=5s) factory reset. Hỗ trợ hoãn chuông (Snooze +9 phút)."),
    ("Bảo mật REST API Cục bộ (FR-SEC-02):", " Web Server LAN Port 80 tích hợp cơ chế xác thực tiêu đề X-API-Key đối soát NVS, so sánh chuỗi thời gian hằng số constant_time_eq() chống Timing Attack (đạt 4/4 test bảo mật)."),
    ("Độ tin cậy & Tự phục hồi:", " Task Supervisor TWDT (15s) giám sát nhịp tim 7 tác vụ qua bitmask 0x7F. Tự động dump lỗi crash vào NVS/RTC khi có sự cố. Đã xử lý triệt để lỗi Deadlock FreeRTOS Mutex đệ quy trong sd_logger.c, đảm bảo bo mạch chạy liên tục ổn định.")
]

for b_title, b_desc in firmware_points:
    bp = doc.add_paragraph(style='List Bullet')
    bp.paragraph_format.space_after = Pt(2)
    r1 = bp.add_run(b_title)
    r1.font.bold = True
    bp.add_run(b_desc)

add_heading_2("2. Phân hệ Máy chủ Đám mây (Cloud Backend & Broker)")
p = doc.add_paragraph()
p.add_run(
    "Máy chủ đám mây Node.js/Express kết hợp Cloud Broker MQTTS EMQX Serverless qua kết nối an toàn TLS cổng 8883. "
    "Toàn bộ logic đã hoàn tất và đạt 20/20 bài kiểm thử tự động (Jest):"
)

backend_points = [
    ("Mô hình dữ liệu Prisma ORM:", " Quản trị bảng quan hệ Gateway, Device, Telemetry, Rule, Schedule, EventLog, User."),
    ("Bảo mật phân quyền & chống Brute-force:", " Xác thực JWT, băm mật khẩu scrypt có muối, chặn brute-force bằng express-rate-limit (tối đa 5 lần claim/15m), chống lỗi BOLA/IDOR."),
    ("Đồng bộ dữ liệu hai chiều & Khử trùng gửi bù:", " Nhận telemetry QoS 1, lọc trùng lặp dữ liệu gửi bù (Store-and-Forward) dựa trên cặp khóa (device_id, timestamp); phát lệnh điều khiển Downlink sinh UUID command_id kèm bắt buộc command_ack từ thiết bị.")
]

for b_title, b_desc in backend_points:
    bp = doc.add_paragraph(style='List Bullet')
    bp.paragraph_format.space_after = Pt(2)
    r1 = bp.add_run(b_title)
    r1.font.bold = True
    bp.add_run(b_desc)

add_heading_2("3. Phân hệ Ứng dụng Web Quản trị (Web Dashboard Frontend)")
p = doc.add_paragraph()
p.add_run(
    "Ứng dụng Web phát triển bằng React 18, Vite và TailwindCSS, tuân thủ tiêu chuẩn thiết kế Impeccable "
    "(loại bỏ hoàn toàn các anti-patterns AI rập khuôn):"
)

web_points = [
    ("Giao diện kỹ thuật tối ưu:", " Bảng màu Industrial IoT (cyan, sky, amber, zinc.750), font hệ thống Native System Font Stack nhẹ và tải tức thì."),
    ("Chức năng hoàn chỉnh:", " Màn hình Đăng nhập/Đăng ký với Auth Guard ProtectedRoute, Dashboard giám sát Gateway/cảm biến thời gian thực, biểu đồ xu hướng Telemetry Chart, giao diện quản lý thiết bị và cài đặt báo thức từ xa."),
    ("Chất lượng mã nguồn:", " Biên dịch Vite sạch 100% trong 2.99 giây, 0 lỗi, kiểm toán đạt 0 anti-patterns.")
]

for b_title, b_desc in web_points:
    bp = doc.add_paragraph(style='List Bullet')
    bp.paragraph_format.space_after = Pt(2)
    r1 = bp.add_run(b_title)
    r1.font.bold = True
    bp.add_run(b_desc)

add_heading_2("4. Kết quả Kiểm toán Thực nghiệm Zero-Trust (11/11/2026)")
p = doc.add_paragraph()
p.add_run(
    "Để đảm bảo tính trung thực học thuật và độ tin cậy của đồ án, sinh viên đã chủ động thực hiện kiểm toán Zero-Trust: "
    "xóa bỏ toàn bộ 10 file mock test ảo của các phiên trước, chỉ công nhận kết quả khi có raw log đo kiểm trực tiếp "
    "trên phần cứng thật kết nối qua cổng /dev/ttyACM0 (lưu vết tại thư mục testing/evidence/2026-11-11/). "
    "Mọi thông số kỹ thuật báo cáo đều là số liệu thực chứng."
)

doc.add_paragraph().paragraph_format.space_after = Pt(8)

# Section III
add_heading_1("III. SO SÁNH CÁC CHỈ SỐ KỸ THUẬT VỚI MỤC TIÊU BAN ĐẦU")

tbl_specs = doc.add_table(rows=8, cols=4)
tbl_specs.alignment = WD_TABLE_ALIGNMENT.CENTER
set_table_borders(tbl_specs, color="CCCCCC")

spec_headers = ["Chỉ số kỹ thuật", "Mục tiêu thiết kế (PGNV / FSD)", "Kết quả thực nghiệm", "Đánh giá"]
for idx, title in enumerate(spec_headers):
    cell = tbl_specs.rows[0].cells[idx]
    set_cell_background(cell, "003366")
    set_cell_margins(cell, 100, 100, 100, 100)
    p = cell.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(0)
    r = p.add_run(title)
    r.font.bold = True
    r.font.size = Pt(10)
    r.font.color.rgb = RGBColor(0xFF, 0xFF, 0xFF)

spec_data = [
    ("Thời gian khởi động hiển thị OLED", "< 1.0 giây", "~0.25 giây (Boot segment 7)", "Vượt chỉ tiêu"),
    ("Lưu giờ khi ngắt nguồn điện", "Độc lập bằng pin RTC", "Chính xác từng giây (RTC DS3231 + Pin CR2032)", "Đạt chuẩn"),
    ("Khả năng tự hành khi mất mạng", "100% (Hiển thị, báo thức, lưu thẻ)", "Tự hành 100%, đệm RAM 50 items + SD FAT32", "Đạt chuẩn"),
    ("Cơ chế gửi bù dữ liệu (Backfill)", "Tự động khi có lại Internet", "Drain thẻ SD và đẩy bù sạch lên MQTTS Broker", "Đạt chuẩn"),
    ("Bộ nhớ RAM khả dụng (Free Heap)", "> 35,000 bytes", "~41,600 – 70,000 bytes (Không rò rỉ RAM)", "Đạt chuẩn"),
    ("Độ tin cậy hệ thống (Task Watchdog)", "Tự phát hiện & phục hồi khi treo", "TWDT 15s phát hiện task stall và dump crash", "Đạt chuẩn"),
    ("An ninh kênh truyền", "MQTTS TLS cổng 8883", "MbedTLS root CA bundle, mã hóa an toàn", "Đạt chuẩn")
]

spec_widths = [Inches(1.8), Inches(1.8), Inches(2.0), Inches(1.1)]

for row_idx, sdata in enumerate(spec_data):
    row = tbl_specs.rows[row_idx + 1]
    bg_color = "F9FBFC" if row_idx % 2 == 1 else "FFFFFF"
    for col_idx, text in enumerate(sdata):
        cell = row.cells[col_idx]
        cell.width = spec_widths[col_idx]
        set_cell_background(cell, bg_color)
        set_cell_margins(cell, 80, 80, 100, 100)
        p = cell.paragraphs[0]
        p.paragraph_format.space_after = Pt(0)
        if col_idx in (1, 3):
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = p.add_run(text)
        r.font.size = Pt(9.5)
        if col_idx == 3:
            r.font.bold = True
            r.font.color.rgb = RGBColor(0x00, 0x88, 0x00)

doc.add_paragraph().paragraph_format.space_after = Pt(8)

# Section IV
add_heading_1("IV. KẾ HOẠCH HOÀN TẤT TRONG CÁC TUẦN CÒN LẠI (TUẦN 16 – 17)")

plan_points = [
    ("Hoàn thiện quyển Báo cáo Đồ án tốt nghiệp:", " Hoàn tất các chương Tổng quan, Phân tích thiết kế, Hiện thực hóa và Đánh giá thực nghiệm; trích xuất biểu đồ kiến trúc hệ thống từ đồ thị tri thức Graphify (856 nodes, 1983 liên kết)."),
    ("Đóng gói mô hình sản phẩm mẫu:", " Lắp ráp hoàn chỉnh mạch ESP32-S3, màn hình OLED 1.5\", mạch RTC DS3231 và khe MicroSD vào hộp bảo vệ; đấu nối cụm rơ-le công suất 4 kênh để demo đóng cắt tải điện thực tế."),
    ("Quay video demo sản phẩm & Slide thuyết trình:", " Thực hiện video minh chứng 3 kịch bản vận hành thực tế: Bình thường ↔ Rút mạng (Offline-First) ↔ Có mạng lại (Gửi bù Store-and-Forward) để trình chiếu trước Hội đồng bảo vệ.")
]

for b_title, b_desc in plan_points:
    bp = doc.add_paragraph(style='List Bullet')
    bp.paragraph_format.space_after = Pt(3)
    r1 = bp.add_run(b_title)
    r1.font.bold = True
    bp.add_run(b_desc)

# Section V
add_heading_1("V. KẾT LUẬN & KIẾN NGHỊ")

p = doc.add_paragraph()
p.add_run(
    "Đồ án đã bám sát 100% các yêu cầu trong Phiếu Giao Nhiệm Vụ, hoàn thành đúng tiến độ các sản phẩm cốt lõi "
    "(Firmware ESP32-S3, Cloud Backend, Web Dashboard) và đã được kiểm chứng ổn định trên bo mạch thật. "
    "Sinh viên kính mong Thầy/Cô Giáo viên hướng dẫn xem xét, đóng góp ý kiến về bố cục quyển báo cáo "
    "và định hướng các nội dung cần nhấn mạnh trong buổi bảo vệ Đồ án tốt nghiệp sắp tới."
)

doc.add_paragraph().paragraph_format.space_after = Pt(16)

# Signature Table
sig_tbl = doc.add_table(rows=1, cols=2)
sig_tbl.alignment = WD_TABLE_ALIGNMENT.CENTER
cell_sl, cell_sr = sig_tbl.rows[0].cells
cell_sl.width = Inches(3.2)
cell_sr.width = Inches(3.2)

p_sl = cell_sl.paragraphs[0]
p_sl.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p_sl.add_run("Ý KIẾN CỦA GIÁO VIÊN HƯỚNG DẪN\n")
r.font.bold = True
r.font.size = Pt(11)
r = p_sl.add_run("(Ký và ghi rõ họ tên)\n\n\n\n\n")
r.font.italic = True
r.font.size = Pt(10)

p_sr = cell_sr.paragraphs[0]
p_sr.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p_sr.add_run("Hà Nội, ngày 11 tháng 11 năm 2026\n")
r.font.italic = True
r.font.size = Pt(11)
r = p_sr.add_run("SINH VIÊN THỰC HIỆN\n")
r.font.bold = True
r.font.size = Pt(11)
r = p_sr.add_run("(Ký và ghi rõ họ tên)\n\n\n\n\n")
r.font.italic = True
r.font.size = Pt(10)

output_path = "report/Bao_Cao_Tien_Do_DATN_SmartClock_Gateway.docx"
doc.save(output_path)
print(f"Successfully generated Word report: {output_path}")
