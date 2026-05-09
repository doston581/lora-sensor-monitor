from flask import Flask, jsonify, render_template
from db import get_conn
import threading
import socket
from datetime import datetime, timedelta
from serial_reader import run

app = Flask(__name__)

# ==========================================
# 守护线程启动 (Daemon Thread)
# ==========================================
# 启动串口读取与入库线程，与 Web 服务并行运行
t = threading.Thread(target=run)
t.daemon = True
t.start()


# ==========================================
# 路由视图 (Routing Views)
# ==========================================
@app.route('/')
def index():
    """
    前端页面主入口 (Frontend Main Entry)
    渲染 templates/index.html
    """
    return render_template('index.html')


@app.route('/data')
def get_data():
    """
    数据接口 (Data API Endpoint)
    供前端 ECharts 轮询调用，返回最新的 20 条数据
    """
    try:
        conn = get_conn()
        cursor = conn.cursor()

        # 降序查询最新的 20 条数据
        cursor.execute("SELECT temperature, humidity, time FROM sensor_data ORDER BY id DESC LIMIT 20")
        rows = cursor.fetchall()
        conn.close()

        # 反转顺序：因为前端图表的 X 轴时间是从左到右（旧到新）
        rows = rows[::-1]

        # 数据清洗与结构化映射 (Data Structuring)
        result = {
            "temp": [r[0] for r in rows],
            "hum": [r[1] for r in rows],
            "time": [str(r[2])[-8:] for r in rows]  # 截取时:分:秒
        }

        return jsonify(result)

    except Exception as e:
        print(f"[API Error] 数据库查询失败: {e}")
        # 返回空数据骨架，防止前端 JS 崩溃
        return jsonify({"temp": [], "hum": [], "time": []}), 500
@app.route('/api/latest_data', methods=['GET'])
def get_latest_data():
    """
    获取最新的一条温湿度记录并判断设备是否离线
    """
    try:
        conn = get_conn()
        cursor = conn.cursor()
        cursor.execute("SELECT node_id, temperature, humidity, time FROM sensor_data ORDER BY id DESC LIMIT 1")
        row = cursor.fetchone()
        conn.close()

        if row is None:
            return jsonify({"status": "error", "message": "No data found"}), 404

        node_id = int(row[0]) if row[0] else 1
        temperature = float(row[1])
        humidity = float(row[2])
        record_time = row[3]

        # 数据新鲜度校验 (超过 10 秒认为离线)
        is_offline = False
        if record_time:
            # 兼容 record_time 是字符串或 datetime 对象的情况
            if isinstance(record_time, str):
                record_time = datetime.strptime(record_time, '%Y-%m-%d %H:%M:%S')
            time_diff = datetime.now() - record_time
            if time_diff.total_seconds() > 10:
                is_offline = True

        if is_offline:
            return jsonify({"status": "error", "message": "Device offline"}), 404

        result = {
            "status": "success",
            "temperature": temperature,
            "humidity": humidity
        }
        return jsonify(result)

    except Exception as e:
        print(f"[API Error] /api/latest_data 查询失败: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500


def get_host_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip

if __name__ == '__main__':
    local_ip = get_host_ip()
    print("=" * 50)
    print(f"[System] Web Server is starting...")
    print(f"-> 电脑本地测试地址: http://127.0.0.1:5000")
    print(f"-> 手机 App 请使用此地址: http://{local_ip}:5000/api/latest_data")
    print("=" * 50)
    # 【核心修复】：必须关闭 debug 和 reloader，否则 Flask 会启动双进程抢夺物理串口
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)