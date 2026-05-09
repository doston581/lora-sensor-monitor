import serial
import re
import time
from db import get_conn

# ==========================================
# 物理层参数强绑定 (Hardware Bindings)
# ==========================================
# 【注意】请将 COM3 改为你 F103 实际的 COM 口
COM_PORT = 'COM11'
# 【致命修正】我们 F103 CubeMX 里配置的是 9600，绝不能用 115200
BAUD_RATE = 9600

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
    print(f"[系统] 成功连接串口 {COM_PORT} @ {BAUD_RATE} bps")
except Exception as e:
    print(f"[致命错误] 串口打开失败，请检查 COM 口是否被占用: {e}")
    ser = None


def parse_data(line):
    """
    业务协议正则解剖引擎 (Regex Parsing Engine)
    目标格式: [Data Parsed] Node: 01 | Temp: 25.3 C | Humi: 60.0 % | RSSI: -45 dBm
    """
    # 使用正则表达式精准捕获数字
    pattern = r"Node:\s*(\d+)\s*\|\s*Temp:\s*([\d\.]+)\s*C\s*\|\s*Humi:\s*([\d\.]+)\s*%"
    match = re.search(pattern, line)

    if match:
        data = {
            'node': match.group(1),
            'temp': float(match.group(2)),
            'hum': float(match.group(3))
        }
        return data
    else:
        return None


def save_to_db(data):
    try:
        conn = get_conn()
        cursor = conn.cursor()

        # 结构化插入 (SQL Injection Safe)
        sql = "INSERT INTO sensor_data (node_id, temperature, humidity) VALUES (%s, %s, %s)"
        cursor.execute(sql, (data['node'], data['temp'], data['hum']))

        conn.commit()
        print(f"[入库成功] 节点:{data['node']} 温度:{data['temp']} 湿度:{data['hum']}")
    except Exception as e:
        print(f"[数据库错误] 写入失败: {e}")
    finally:
        if 'conn' in locals() and conn.open:
            conn.close()


def run():
    if ser is None:
        return

    while True:
        try:
            # 阻塞读取并解码
            raw_bytes = ser.readline()
            if not raw_bytes:
                continue

            line = raw_bytes.decode('utf-8', errors='ignore').strip()

            # 过滤无关的日志信息，只处理包含真实数据的行
            if "[Data Parsed]" in line:
                print(f"➔ 捕获射频帧: {line}")
                data = parse_data(line)

                if data:
                    save_to_db(data)
                else:
                    print("    ❌ 格式正则匹配失败，数据丢弃")

        except Exception as e:
            print(f"[运行时错误] 读取线程异常: {e}")
            time.sleep(1)  # 防止死循环刷屏