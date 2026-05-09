import serial
import time
import random

# ==========================================
# 【注意】将 COM10 改为你 VSPD 虚拟出来用于发送的串口
# 比如 VSPD 虚拟了 COM10 和 COM11 互通
# 那么 serial_reader.py 连 COM11，这里就填 COM10
# ==========================================
COM_PORT = 'COM10'
BAUD_RATE = 9600

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE)
    print(f"✅ 模拟传感器启动成功，已连接 {COM_PORT} @ {BAUD_RATE} bps")
    print("正在持续发送随机模拟数据 (Ctrl+C 停止)...\n")

    while True:
        # 1. 生成随机但合理的温湿度数据
        temp = round(random.uniform(20.0, 35.0), 1)  # 20.0 到 35.0 度
        hum = round(random.uniform(40.0, 80.0), 1)  # 40.0% 到 80.0% 湿度

        # 2. 严格按照 serial_reader.py 的正则解析格式拼接字符串
        # 目标格式: [Data Parsed] Node: 01 | Temp: 25.3 C | Humi: 60.0 % | RSSI: -45 dBm
        data_str = f"[Data Parsed] Node: 01 | Temp: {temp} C | Humi: {hum} % | RSSI: -45 dBm\n"

        # 3. 发送到串口
        ser.write(data_str.encode('utf-8'))
        print(f"➔ 模拟发送: {data_str.strip()}")

        # 4. 定时发送 (例如每 2 秒发送一次)
        time.sleep(2)

except Exception as e:
    print(f"❌ 模拟器运行失败，请检查串口是否正确且未被占用: {e}")
