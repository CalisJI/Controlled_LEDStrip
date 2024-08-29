# Project to controll LED Strip using ESP32 via UPD protocol

# Configuration:
+ Connect power to LED Strp
+ Connect Data wire to GPIO2 of ESP32

![](/img/strip.png)


### Data frame RGB565:
`uint16_t[]` ex: `[0xF800, 0x07E0, 0x001F, ...] hoặc [64896, 2016, 31, ...]`

+ Led Number set by array's size
+ LED's color set by item value
### LED Settings:
![](/img/led1.jpg)
- **LEDs:** Set LED number
- **Brightness:** Set LED brightness
- **IP Address:** *default:* 192.168.1.10

- *Power up ESP* 
- *Enter Ip address on browser to connect to the LED settings page after power on ESP32*

### Example Sender create random led number and corresponding color:
```python
import socket
import random
import struct
def rgb888_to_rgb565(r, g, b):
    """Chuyển đổi từ RGB888 sang RGB565"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
# Hàm tạo màu RGB565 ngẫu nhiên
def generate_random_rgb565():
    red   = random.randint(0, 255) 
    green = random.randint(0, 255)  
    blue  = random.randint(0, 255) 
    return rgb888_to_rgb565(red,green,blue)
# Tạo 100 màu RGB565 ngẫu nhiên
def generate_rgb565_colors(num_colors):
    return [generate_random_rgb565() for _ in range(num_colors)]


# Chia dữ liệu thành các khối nhỏ hơn
def chunk_data(data, chunk_size):
    return [data[i:i + chunk_size] for i in range(0, len(data), chunk_size)]

# Gửi dữ liệu qua UDP
def send_data_via_udp(ip, port, data, chunk_size):
    chunk_size = 1450 # Kích thước gói tối đa để tránh tràn UDP
    num_packets = (len(data) * 2 + chunk_size - 1) // chunk_size
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print(num_packets)
    for i in range(num_packets):
        start_index = i * chunk_size // 2
        end_index = min((i + 1) * chunk_size // 2, len(data))
        packet = struct.pack('!' + 'H' * (end_index - start_index), *data[start_index:end_index])
        print(packet)
        sock.sendto(packet, (ip, port))

if __name__ == "__main__":
    ESP32_IP = '192.168.1.10'  # Địa chỉ IP của ESP32
    ESP32_PORT = 5000          # Cổng UDP của ESP32
    CHUNK_SIZE = 1450 // 2      # Mỗi màu RGB565 chiếm 2 byte

    # Tạo mảng 100 màu RGB565 ngẫu nhiên
    colors = generate_rgb565_colors(12)

    # Gửi dữ liệu qua UDP với kích thước khối 1450 byte
    send_data_via_udp(ESP32_IP, ESP32_PORT, colors, CHUNK_SIZE)
```

# References:
**FastLED**: https://github.com/FastLED/FastLED
